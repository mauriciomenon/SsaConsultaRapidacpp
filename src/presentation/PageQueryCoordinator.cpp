#include "presentation/PageQueryCoordinator.h"

#include "presentation/SsaTablePageFormatter.h"

#include <QtConcurrent>

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    PageQueryCoordinator::PageQueryCoordinator(std::shared_ptr<query::SsaQueryService> queryService,
                                               QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)) {
        if (!queryService_) {
            throw std::invalid_argument("query service is required");
        }
        connect(&watcher_, &QFutureWatcher<PageQueryResult>::finished, this,
                &PageQueryCoordinator::finishActiveRequest);
    }

    PageQueryCoordinator::~PageQueryCoordinator() {
        cancel();
        activeRequest_.reset();
        pendingRequest_.reset();
        requestRunning_ = false;
        finishing_ = false;
        activeCanceled_ = true;
        explicitCancelRequested_ = true;
    }

    void PageQueryCoordinator::run(domain::SsaPageRequest request) {
        if (activeRequest_ && *activeRequest_ == request && !activeCanceled_) {
            return;
        }
        if (pendingRequest_ && *pendingRequest_ == request) {
            return;
        }
        if (requestRunning_ || finishing_ || watcher_.isRunning()) {
            // Latest-wins: intermediate search states are intentionally replaced.
            activeCanceled_ = true;
            explicitCancelRequested_ = false;
            if (activeCancelToken_) {
                activeCancelToken_->store(true, std::memory_order_relaxed);
            }
            watcher_.cancel();
            pendingRequest_ = std::move(request);
            emit replaced();
            return;
        }

        start(std::move(request));
    }

    void PageQueryCoordinator::cancel() {
        activeCanceled_ = true;
        explicitCancelRequested_ = true;
        if (activeCancelToken_) {
            activeCancelToken_->store(true, std::memory_order_relaxed);
        }
        watcher_.cancel();
        pendingRequest_.reset();
    }

    void PageQueryCoordinator::start(domain::SsaPageRequest request) {
        activeRequest_ = request;
        requestRunning_ = true;
        activeCanceled_ = false;
        explicitCancelRequested_ = false;
        activeCancelToken_ = std::make_shared<std::atomic_bool>(false);

        const auto queryService = queryService_;
        const auto cancelToken = activeCancelToken_;
        auto displayColumns = columnCatalog_.resolveAll(request.visibleColumns);
        const bool needTotalRowsAll = !totalRowsAllKnown_;
        const auto cachedTotalRowsAll = totalRowsAll_;
        watcher_.setFuture(QtConcurrent::run(
            [queryService, request = std::move(request), displayColumns = std::move(displayColumns),
             cancelToken, needTotalRowsAll, cachedTotalRowsAll]() mutable {
                if (!queryService) {
                    throw std::runtime_error("query service no longer available");
                }
                auto totalRowsRequest = domain::SsaPageRequest{};
                totalRowsRequest.excludeScaSesSte = domain::kDefaultExcludeScaSesSte;
                auto page = queryService->search(request);
                if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                    return PageQueryResult{domain::SsaPageResult{}, 0,   false, {},
                                           SsaTableDisplayValues{}, true};
                }
                std::size_t totalRowsAll = cachedTotalRowsAll;
                bool totalRowsAllComputed = false;
                if (needTotalRowsAll) {
                    totalRowsAll = queryService->count(totalRowsRequest);
                    totalRowsAllComputed = true;
                }
                if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                    return PageQueryResult{domain::SsaPageResult{}, totalRowsAll,
                                           totalRowsAllComputed,    {},
                                           SsaTableDisplayValues{}, true};
                }
                auto displayValues =
                    SsaTablePageFormatter::format(page, displayColumns, cancelToken);
                if (!displayValues) {
                    return PageQueryResult{domain::SsaPageResult{}, totalRowsAll,
                                           totalRowsAllComputed,    {},
                                           SsaTableDisplayValues{}, true};
                }
                return PageQueryResult{std::move(page),           totalRowsAll,
                                       totalRowsAllComputed,      std::move(displayColumns),
                                       std::move(*displayValues), false};
            }));
        emit started();
    }

    void PageQueryCoordinator::finishActiveRequest() {
        if (!activeRequest_) {
            return;
        }
        finishing_ = true;
        if (!activeCanceled_ && !watcher_.isCanceled()) {
            try {
                auto result = watcher_.result();
                // Cache the grand total as soon as it is computed, even when the
                // request is later superseded/cancelled: the COUNT(*) over the
                // whole table is stable for the session and re-running it on every
                // keystroke is the expensive part.
                if (result.totalRowsAllComputed) {
                    totalRowsAll_ = result.totalRowsAll;
                    totalRowsAllKnown_ = true;
                }
                if (result.canceled) {
                    if (explicitCancelRequested_ && !pendingRequest_) {
                        emit canceled();
                    }
                } else {
                    emit succeeded(std::move(result), *activeRequest_);
                }
            } catch (const std::length_error& exc) {
                emit failed(QString::fromUtf8(exc.what()));
            } catch (const std::exception& exc) {
                emit failed(QString::fromUtf8(exc.what()));
            } catch (...) {
                emit failed("Falha interna ao consultar dados");
            }
        }

        if (pendingRequest_) {
            auto nextRequest = std::move(*pendingRequest_);
            pendingRequest_.reset();
            start(std::move(nextRequest));
            finishing_ = false;
        } else {
            activeRequest_.reset();
            requestRunning_ = false;
            finishing_ = false;
        }
    }

} // namespace ssa::presentation
