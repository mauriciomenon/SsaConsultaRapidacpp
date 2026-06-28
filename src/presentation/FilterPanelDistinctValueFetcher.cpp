#include "presentation/FilterPanelDistinctValueFetcher.h"

#include <QCoreApplication>
#include <QtConcurrent>

#include <atomic>
#include <memory>
#include <utility>

namespace ssa::presentation {

    FilterPanelDistinctValueFetcher::FilterPanelDistinctValueFetcher(
        std::shared_ptr<query::SsaQueryService> queryService, QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)) {
        connect(&watcher_, &QFutureWatcher<void>::finished, this, [this] { onWatcherFinished(); });
    }

    FilterPanelDistinctValueFetcher::~FilterPanelDistinctValueFetcher() {
        // Drop any queued request so onWatcherFinished does not start a new worker
        // during teardown, then drain the in-flight one.
        hasPendingRequest_ = false;
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
            QCoreApplication::processEvents();
        }
    }

    void
    FilterPanelDistinctValueFetcher::requestValues(const domain::DistinctValuesRequest& request,
                                                   std::uint64_t requestToken) {
        if (!queryService_) {
            emit this->valuesReady(requestToken, {});
            return;
        }
        // If a task is in flight, do NOT cancel+setFuture immediately: the
        // previous runnable may still be constructing/running and racing its vptr
        // with the new setFuture call (TSan: data race on vptr). Instead, mark the
        // old request cancelled and queue this one to run when the current worker
        // finishes (handled in onWatcherFinished).
        if (watcher_.isRunning()) {
            if (activeCancelToken_) {
                activeCancelToken_->store(true, std::memory_order_relaxed);
            }
            pendingRequest_ = request;
            pendingRequestToken_ = requestToken;
            hasPendingRequest_ = true;
            return;
        }

        startWorker(request, requestToken);
    }

    void FilterPanelDistinctValueFetcher::startWorker(const domain::DistinctValuesRequest& request,
                                                      std::uint64_t requestToken) {
        activeCancelToken_ = std::make_shared<std::atomic_bool>(false);
        const auto cancelToken = activeCancelToken_;
        const auto service = queryService_;
        auto requestCopy = request;
        // Result is carried in a shared_ptr captured by value; the worker writes
        // through it and onWatcherFinished (owner thread) moves it out. The future
        // is void, so QFutureInterface<T>'s ResultStore (the original TSan race)
        // is never instantiated.
        const auto result = std::make_shared<std::vector<std::string>>();
        std::lock_guard<std::mutex> lock(resultMutex_);
        activeResult_ = result;

        watcher_.setFuture(
            QtConcurrent::run([service, requestCopy = std::move(requestCopy), cancelToken, result] {
                // The query already projects TRIM(COALESCE(col,'')) and filters empties,
                // so the values arrive normalized; just propagate them unless cancelled.
                if (cancelToken->load(std::memory_order_relaxed)) {
                    return;
                }
                *result = service->distinctValues(requestCopy);
            }));
        activeRequestToken_ = requestToken;
    }

    void FilterPanelDistinctValueFetcher::onWatcherFinished() {
        if (!watcher_.isFinished()) {
            return;
        }
        // A superseded request flipped the token; drop this stale result and run
        // the pending one if any (keeps the latest user intent).
        const bool cancelled =
            activeCancelToken_ && activeCancelToken_->load(std::memory_order_relaxed);
        if (!cancelled) {
            std::vector<std::string> values;
            std::shared_ptr<std::vector<std::string>> result;
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                result = activeResult_;
                activeResult_.reset();
            }
            if (result) {
                values = std::move(*result);
            }
            emit this->valuesReady(activeRequestToken_, std::move(values));
        }
        if (hasPendingRequest_) {
            const auto pending = std::move(pendingRequest_);
            const auto token = pendingRequestToken_;
            hasPendingRequest_ = false;
            startWorker(pending, token);
        }
    }

} // namespace ssa::presentation
