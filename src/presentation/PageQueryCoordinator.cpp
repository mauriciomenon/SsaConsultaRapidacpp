#include "presentation/PageQueryCoordinator.h"

#include "ports/OperationError.h"
#include "presentation/AsyncOperationErrorLog.h"
#include "presentation/SsaTablePageFormatter.h"

#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    PageQueryCoordinator::PageQueryCoordinator(std::shared_ptr<query::SsaQueryService> queryService,
                                               QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)) {
        if (!queryService_) {
            throw std::invalid_argument("query service is required");
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
        auto* current = latestOperation();
        if (current != nullptr && current->request == request &&
            !current->stopSource.stop_requested()) {
            return;
        }
        const bool replacing = current != nullptr;
        for (const auto& operation : operations_) {
            if (!operation->completed) {
                stopOperation(*operation);
            }
        }
        if (replacing) {
            emit replaced();
        }
        start(std::move(request));
    }

    void PageQueryCoordinator::cancel() {
        if (state_ != State::Running) {
            return;
        }
        auto* operation = latestOperation();
        if (operation == nullptr || operation->explicitlyCanceled) {
            return;
        }
        operation->explicitlyCanceled = true;
        setState(State::Canceling);
        stopOperation(*operation);
    }

    void PageQueryCoordinator::invalidateTotalRowsAll() {
        totalRowsAllKnown_ = false;
        totalRowsAll_ = 0;
    }

    void PageQueryCoordinator::start(domain::SsaPageRequest request) {
        const auto queryService = queryService_;
        auto operation = std::make_unique<Operation>();
        operation->id = ++nextOperationId_;
        operation->request = request;
        operation->resultState = std::make_shared<PageQueryResultState>();
        operation->cancelToken = std::make_shared<std::atomic_bool>(false);
        const auto operationId = operation->id;
        const auto stopToken = operation->stopSource.get_token();
        const auto cancelToken = operation->cancelToken;
        const auto resultState = operation->resultState;
        connect(&operation->watcher, &QFutureWatcher<void>::finished, this,
                [this, operationId] { finishOperation(operationId); });
        auto* watcher = &operation->watcher;
        latestOperationId_ = operationId;
        operations_.push_back(std::move(operation));
        emit activeOperationsChanged();
        setState(State::Running);

        auto displayColumns = columnCatalog_.resolveAll(request.visibleColumns);
        const bool needTotalRowsAll = !totalRowsAllKnown_;
        const auto cachedTotalRowsAll = totalRowsAll_;
        watcher->setFuture(QtConcurrent::run([queryService, request = std::move(request),
                                              displayColumns = std::move(displayColumns), stopToken,
                                              cancelToken, resultState, needTotalRowsAll,
                                              cachedTotalRowsAll]() mutable {
            try {
                if (!queryService) {
                    throw std::runtime_error("query service no longer available");
                }
                auto result = [&]() -> PageQueryResult {
                    try {
                        auto totalRowsRequest = domain::SsaPageRequest{};
                        totalRowsRequest.excludeScaSesSte = domain::kDefaultExcludeScaSesSte;
                        auto page = queryService->search(request, stopToken);
                        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                            return {domain::SsaPageResult{}, 0,   false, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        std::size_t totalRowsAll = cachedTotalRowsAll;
                        bool totalRowsAllComputed = false;
                        if (needTotalRowsAll) {
                            totalRowsAll = queryService->count(totalRowsRequest, stopToken);
                            totalRowsAllComputed = true;
                        }
                        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                            return {domain::SsaPageResult{}, totalRowsAll, totalRowsAllComputed, {},
                                    SsaTableDisplayValues{}, true};
                        }
                        auto displayValues =
                            SsaTablePageFormatter::format(page, displayColumns, cancelToken);
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
        emit started();
    }

    void PageQueryCoordinator::finishOperation(const std::uint64_t operationId) {
        const auto found = std::ranges::find_if(operations_, [operationId](const auto& operation) {
            return operation->id == operationId;
        });
        if (found == operations_.end()) {
            return;
        }
        auto& operation = **found;
        operation.completed = true;
        emit activeOperationsChanged();
        const bool isLatest = operation.id == latestOperationId_;
        if (!shuttingDown_ && isLatest &&
            (operation.explicitlyCanceled || !operation.watcher.isCanceled())) {
            finishing_ = true;
            try {
                std::optional<PageQueryResult> result;
                std::exception_ptr error;
                if (operation.resultState) {
                    std::scoped_lock lock(operation.resultState->mutex);
                    result = std::move(operation.resultState->result);
                    error = operation.resultState->error;
                }
                // Cache the grand total as soon as it is computed, even when the
                // request is later superseded/cancelled: the COUNT(*) over the
                // whole table is stable for the session and re-running it on every
                // keystroke is the expensive part.
                if (result && result->totalRowsAllComputed) {
                    totalRowsAll_ = result->totalRowsAll;
                    totalRowsAllKnown_ = true;
                }
                if (operation.explicitlyCanceled || operation.stopSource.stop_requested()) {
                    logAsyncOperationError("Page query failed after cancellation:", error);
                    emit canceled();
                } else {
                    if (error) {
                        std::rethrow_exception(error);
                    }
                    if (!result) {
                        throw std::runtime_error("page query completed without a result");
                    }
                    if (result->canceled) {
                        emit canceled();
                    } else {
                        emit succeeded(std::move(*result), operation.request);
                    }
                }
            } catch (const ports::OperationError& exc) {
                qWarning().noquote()
                    << "Page query failed:" << QString::fromStdString(exc.diagnostic());
                emit failed(QString::fromUtf8(exc.what()));
            } catch (const std::system_error& exc) {
                if (operation.explicitlyCanceled &&
                    exc.code() == std::make_error_code(std::errc::operation_canceled)) {
                    emit canceled();
                } else {
                    qWarning().noquote() << "Page query failed:" << exc.what();
                    emit failed("Falha ao consultar dados");
                }
            } catch (const std::exception& exc) {
                qWarning().noquote() << "Page query failed:" << exc.what();
                emit failed("Falha ao consultar dados");
            } catch (...) {
                qWarning() << "Page query failed: unknown exception";
                emit failed("Falha ao consultar dados");
            }
            finishing_ = false;
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
