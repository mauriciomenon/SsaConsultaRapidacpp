#include "presentation/PageQueryCoordinator.h"

#include "diagnostics/StartupTrace.h"
#include "ports/OperationError.h"
#include "presentation/AsyncOperationErrorLog.h"
#include "presentation/SsaTablePageFormatter.h"

#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace ssa::presentation {

    namespace {
        std::atomic_bool startupQueryClaimed{false};
    }

    PageQueryCoordinator::PageQueryCoordinator(std::shared_ptr<ports::ISsaBrowsePort> browsePort,
                                               QObject* parent)
        : QObject(parent), browsePort_(std::move(browsePort)) {
        if (!browsePort_) {
            throw std::invalid_argument("browse port is required");
        }
    }

    PageQueryCoordinator::~PageQueryCoordinator() {
        shuttingDown_ = true;
        for (const auto& operation : operations_) {
            disconnect(&operation->watcher, nullptr, this, nullptr);
            stopOperation(*operation);
        }
        operations_.clear();
    }

    PageQueryCoordinator::State PageQueryCoordinator::state() const {
        return state_;
    }

    bool PageQueryCoordinator::hasActiveOperations() const {
        return std::ranges::any_of(operations_,
                                   [](const auto& operation) { return !operation->completed; });
    }

    void PageQueryCoordinator::run(domain::SsaPageRequest request) {
        if (shuttingDown_ || state_ == State::Canceling || finishing_) {
            return;
        }
        invalidateCacheForQuery(request);
        auto* current = latestOperation();
        if (current != nullptr && current->request == request &&
            current->generation == cacheGeneration_ && !current->stopSource.stop_requested()) {
            return;
        }
        const bool replacing = current != nullptr;
        for (const auto& operation : operations_) {
            if (!operation->completed) {
                stopOperation(*operation);
            }
        }
        if (replacing) {
            if (current->startupTrace) {
                diagnostics::traceStartupEvent(
                    "publish_terminal",
                    "thread=gui request_id=" + std::to_string(current->id) + " outcome=replaced");
                current->startupTrace = false;
                startupQueryClaimed.store(false, std::memory_order_relaxed);
            }
            emit replaced();
        }
        if (auto cached = takeCachedPage(request)) {
            cachePage(request, *cached);
            latestOperationId_ = ++nextOperationId_;
            setState(State::Running);
            finishing_ = true;
            emit started();
            if (cached->totalRowsAllComputed) {
                totalRowsAll_ = cached->totalRowsAll;
                totalRowsAllKnown_ = true;
            }
            const auto totalRows = cached->page.totalRows;
            emit succeeded(std::move(*cached), request);
            finishing_ = false;
            setState(State::Idle);
            const bool suppressPrefetch = prefetchCancellationRequested_;
            prefetchCancellationRequested_ = false;
            if (!suppressPrefetch) {
                startPrefetchWindow(request, totalRows);
            }
            return;
        }
        start(std::move(request), false);
    }

    void PageQueryCoordinator::cancel() {
        for (const auto& operation : operations_) {
            if (!operation->completed && operation->prefetch) {
                stopOperation(*operation);
            }
        }

        if (state_ != State::Running) {
            return;
        }
        auto* operation = latestOperation();
        if (operation == nullptr) {
            if (finishing_) {
                prefetchCancellationRequested_ = true;
            }
            return;
        }
        if (operation->explicitlyCanceled) {
            return;
        }
        operation->explicitlyCanceled = true;
        setState(State::Canceling);
        stopOperation(*operation);
    }

    void PageQueryCoordinator::invalidateTotalRowsAll() {
        totalRowsAllKnown_ = false;
        totalRowsAll_ = 0;
        ++cacheGeneration_;
        pageCache_.clear();
        cacheQuery_.reset();
    }

    void PageQueryCoordinator::start(domain::SsaPageRequest request, const bool prefetch) {
        const auto browsePort = browsePort_;
        auto operation = std::make_unique<Operation>();
        operation->id = ++nextOperationId_;
        operation->generation = cacheGeneration_;
        operation->request = request;
        operation->prefetch = prefetch;
        operation->startupTrace = !prefetch && diagnostics::startupTraceEnabled() &&
                                  !startupQueryClaimed.exchange(true, std::memory_order_relaxed);
        operation->resultState = std::make_shared<PageQueryResultState>();
        operation->cancelToken = std::make_shared<std::atomic_bool>(false);
        const auto operationId = operation->id;
        const auto stopToken = operation->stopSource.get_token();
        const auto cancelToken = operation->cancelToken;
        const auto resultState = operation->resultState;
        const bool startupTrace = operation->startupTrace;
        connect(&operation->watcher, &QFutureWatcher<void>::finished, this,
                [this, operationId] { finishOperation(operationId); });
        auto* watcher = &operation->watcher;
        if (!prefetch) {
            latestOperationId_ = operationId;
        }
        operations_.push_back(std::move(operation));
        emit activeOperationsChanged();
        if (!prefetch) {
            setState(State::Running);
        }

        auto displayColumns = columnCatalog_.resolveAll(request.visibleColumns);
        const bool needTotalRowsAll = !prefetch && !totalRowsAllKnown_;
        const auto cachedTotalRowsAll = totalRowsAll_;
        if (startupTrace) {
            diagnostics::traceStartupEvent("query_start",
                                           "thread=gui request_id=" + std::to_string(operationId));
        }
        watcher->setFuture(QtConcurrent::run([browsePort, request = std::move(request),
                                              displayColumns = std::move(displayColumns), stopToken,
                                              cancelToken, resultState, needTotalRowsAll,
                                              cachedTotalRowsAll, startupTrace,
                                              operationId]() mutable {
            try {
                if (!browsePort) {
                    throw std::runtime_error("browse port no longer available");
                }
                auto result = [&]() -> PageQueryResult {
                    try {
                        auto totalRowsRequest = domain::SsaPageRequest{};
                        totalRowsRequest.excludeScaSesSte = domain::kDefaultExcludeScaSesSte;
                        auto page = browsePort->page(request, stopToken);
                        if (startupTrace) {
                            diagnostics::traceStartupEvent(
                                "page_returned",
                                "thread=worker request_id=" + std::to_string(operationId) +
                                    " rows=" + std::to_string(page.rows.size()) +
                                    " total=" + std::to_string(page.totalRows));
                        }
                        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                            return {domain::SsaPageResult{}, 0,   false, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        std::size_t totalRowsAll = cachedTotalRowsAll;
                        bool totalRowsAllComputed = false;
                        if (needTotalRowsAll) {
                            totalRowsAll = browsePort->count(totalRowsRequest, stopToken);
                            totalRowsAllComputed = true;
                        }
                        if (startupTrace) {
                            diagnostics::traceStartupEvent(
                                needTotalRowsAll ? "count_returned" : "count_skipped",
                                "thread=worker request_id=" + std::to_string(operationId) +
                                    " total=" + std::to_string(totalRowsAll));
                        }
                        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                            return {domain::SsaPageResult{}, totalRowsAll, totalRowsAllComputed, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        if (startupTrace) {
                            diagnostics::traceStartupEvent("format_start",
                                                           "thread=worker request_id=" +
                                                               std::to_string(operationId));
                        }
                        auto displayValues =
                            SsaTablePageFormatter::format(page, displayColumns, cancelToken);
                        if (startupTrace) {
                            diagnostics::traceStartupEvent("format_end",
                                                           "thread=worker request_id=" +
                                                               std::to_string(operationId));
                        }
                        if (!displayValues) {
                            return {domain::SsaPageResult{}, totalRowsAll, totalRowsAllComputed, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        return {std::move(page),           totalRowsAll,
                                totalRowsAllComputed,      std::move(displayColumns),
                                std::move(*displayValues), false};
                    } catch (const std::system_error& error) {
                        if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                            return {domain::SsaPageResult{}, 0,   false, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        throw;
                    }
                }();
                std::scoped_lock lock(resultState->mutex);
                resultState->result = std::move(result);
            } catch (...) {
                std::scoped_lock lock(resultState->mutex);
                resultState->error = std::current_exception();
            }
        }));
        if (!prefetch) {
            emit started();
        }
    }

    void PageQueryCoordinator::finishOperation(const std::uint64_t operationId) {
        const auto found = std::ranges::find_if(operations_, [operationId](const auto& operation) {
            return operation->id == operationId;
        });
        if (found == operations_.end()) {
            return;
        }
        auto& operation = **found;
        const auto traceTerminal = [&operation](const std::string_view outcome,
                                                const std::size_t rows = 0,
                                                const std::size_t total = 0) {
            if (!operation.startupTrace) {
                return;
            }
            diagnostics::traceStartupEvent("publish_terminal",
                                           "thread=gui request_id=" + std::to_string(operation.id) +
                                               " outcome=" + std::string{outcome} +
                                               " rows=" + std::to_string(rows) +
                                               " total=" + std::to_string(total));
            operation.startupTrace = false;
            if (outcome != "success") {
                startupQueryClaimed.store(false, std::memory_order_relaxed);
            }
        };
        operation.completed = true;
        emit activeOperationsChanged();
        if (operation.prefetch) {
            std::optional<PageQueryResult> result;
            {
                std::scoped_lock lock(operation.resultState->mutex);
                result = std::move(operation.resultState->result);
            }
            if (result && operation.generation == cacheGeneration_ && !result->canceled &&
                !operation.stopSource.stop_requested()) {
                cachePage(operation.request, *result);
            }
            QMetaObject::invokeMethod(
                this, [this] { pruneCompletedOperations(); }, Qt::QueuedConnection);
            return;
        }
        const bool isLatest = operation.id == latestOperationId_;
        const bool isCurrentGeneration = operation.generation == cacheGeneration_;
        std::optional<PageQueryResult> result;
        std::exception_ptr error;
        if (operation.resultState) {
            std::scoped_lock lock(operation.resultState->mutex);
            result = std::move(operation.resultState->result);
            error = operation.resultState->error;
        }
        if (!shuttingDown_ && isCurrentGeneration && result && result->totalRowsAllComputed) {
            totalRowsAll_ = result->totalRowsAll;
            totalRowsAllKnown_ = true;
        }
        if (!shuttingDown_ && isLatest && isCurrentGeneration &&
            (operation.explicitlyCanceled || !operation.watcher.isCanceled())) {
            finishing_ = true;
            try {
                if (operation.explicitlyCanceled || operation.stopSource.stop_requested()) {
                    logAsyncOperationError("Page query failed after cancellation:", error);
                    emit canceled();
                    traceTerminal("canceled");
                } else {
                    if (error) {
                        std::rethrow_exception(error);
                    }
                    if (!result) {
                        throw std::runtime_error("page query completed without a result");
                    }
                    if (result->canceled) {
                        emit canceled();
                        traceTerminal("canceled");
                    } else {
                        cachePage(operation.request, *result);
                        const auto totalRows = result->page.totalRows;
                        const auto rowCount = result->page.rows.size();
                        emit succeeded(std::move(*result), operation.request);
                        traceTerminal("success", rowCount, totalRows);
                        const bool suppressPrefetch = prefetchCancellationRequested_;
                        prefetchCancellationRequested_ = false;
                        if (!suppressPrefetch) {
                            startPrefetchWindow(operation.request, totalRows);
                        }
                    }
                }
            } catch (const ports::OperationError& exc) {
                qWarning().noquote()
                    << "Page query failed:" << QString::fromStdString(exc.diagnostic());
                emit failed(QString::fromUtf8(exc.what()));
                traceTerminal("failed");
            } catch (const std::system_error& exc) {
                if (operation.explicitlyCanceled &&
                    exc.code() == std::make_error_code(std::errc::operation_canceled)) {
                    emit canceled();
                    traceTerminal("canceled");
                } else {
                    qWarning().noquote() << "Page query failed:" << exc.what();
                    emit failed("Falha ao consultar dados");
                    traceTerminal("failed");
                }
            } catch (const std::exception& exc) {
                qWarning().noquote() << "Page query failed:" << exc.what();
                emit failed("Falha ao consultar dados");
                traceTerminal("failed");
            } catch (...) {
                qWarning() << "Page query failed: unknown exception";
                emit failed("Falha ao consultar dados");
                traceTerminal("failed");
            }
            finishing_ = false;
            setState(State::Idle);
        } else if (isLatest && !hasActiveOperations()) {
            setState(State::Idle);
        }
        QMetaObject::invokeMethod(
            this, [this] { pruneCompletedOperations(); }, Qt::QueuedConnection);
    }

    void PageQueryCoordinator::stopOperation(Operation& operation) {
        if (operation.cancelToken) {
            operation.cancelToken->store(true, std::memory_order_relaxed);
        }
        operation.stopSource.request_stop();
        operation.watcher.cancel();
    }

    void PageQueryCoordinator::pruneCompletedOperations() {
        std::erase_if(operations_, [](const auto& operation) { return operation->completed; });
    }

    void PageQueryCoordinator::startPrefetchWindow(const domain::SsaPageRequest& request,
                                                   const std::size_t totalRows) {
        if (request.pageSize == 0) {
            return;
        }
        const auto pages = domain::pageCount(totalRows, request.pageSize);
        for (std::size_t offset = 1; offset <= kPrefetchPageCount; ++offset) {
            if (request.pageIndex >= pages || offset > pages - request.pageIndex - 1) {
                break;
            }
            auto prefetchRequest = request;
            prefetchRequest.pageIndex += offset;
            if (hasCachedPage(prefetchRequest)) {
                continue;
            }
            const auto active = std::ranges::any_of(operations_, [&](const auto& operation) {
                return !operation->completed && operation->request == prefetchRequest;
            });
            if (!active) {
                start(std::move(prefetchRequest), true);
            }
        }
    }

    void PageQueryCoordinator::cachePage(const domain::SsaPageRequest& request,
                                         const PageQueryResult& result) {
        if (result.canceled) {
            return;
        }
        auto fingerprint = request;
        fingerprint.pageIndex = 0;
        if (!cacheQuery_ || *cacheQuery_ != fingerprint) {
            pageCache_.clear();
            cacheQuery_ = fingerprint;
        }
        std::erase_if(pageCache_, [&](const auto& cached) { return cached.request == request; });
        pageCache_.push_back({request, result});
        while (pageCache_.size() > kMaxCachedPages) {
            pageCache_.pop_front();
        }
    }

    std::optional<PageQueryResult>
    PageQueryCoordinator::takeCachedPage(const domain::SsaPageRequest& request) {
        const auto found = std::ranges::find_if(
            pageCache_, [&](const auto& cached) { return cached.request == request; });
        if (found == pageCache_.end()) {
            return std::nullopt;
        }
        auto result = std::move(found->result);
        pageCache_.erase(found);
        return result;
    }

    void PageQueryCoordinator::invalidateCacheForQuery(const domain::SsaPageRequest& request) {
        auto fingerprint = request;
        fingerprint.pageIndex = 0;
        if (cacheQuery_ && *cacheQuery_ != fingerprint) {
            ++cacheGeneration_;
            pageCache_.clear();
            cacheQuery_.reset();
        }
    }

    bool PageQueryCoordinator::hasCachedPage(const domain::SsaPageRequest& request) const {
        return std::ranges::any_of(pageCache_,
                                   [&](const auto& cached) { return cached.request == request; });
    }

    void PageQueryCoordinator::setState(const State state) {
        if (state_ == state) {
            return;
        }
        state_ = state;
        emit stateChanged(state_);
    }

    PageQueryCoordinator::Operation* PageQueryCoordinator::latestOperation() {
        const auto found = std::ranges::find_if(operations_, [this](const auto& operation) {
            return operation->id == latestOperationId_ && !operation->completed &&
                   !operation->explicitlyCanceled;
        });
        return found == operations_.end() ? nullptr : found->get();
    }

} // namespace ssa::presentation
