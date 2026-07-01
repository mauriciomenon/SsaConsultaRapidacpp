#include "presentation/FilterPanelDistinctValueFetcher.h"

#include <QCoreApplication>
#include <QtConcurrent>

#include <algorithm>
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
        clearPendingRequests();
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
            QCoreApplication::processEvents();
        }
    }

    void FilterPanelDistinctValueFetcher::clearPendingRequests() {
        pendingRequests_.clear();
        if (activeCancelToken_) {
            activeCancelToken_->store(true, std::memory_order_relaxed);
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
        // with the new setFuture call (TSan: data race on vptr). Instead, queue
        // this one to run when the current worker finishes.
        if (activeRequestInFlight_) {
            const auto sameColumn = [&request](const PendingRequest& pending) {
                return pending.request.columnKey == request.columnKey;
            };
            if (const auto found =
                    std::find_if(pendingRequests_.begin(), pendingRequests_.end(), sameColumn);
                found != pendingRequests_.end()) {
                *found = PendingRequest{request, requestToken};
            } else {
                pendingRequests_.push_back(PendingRequest{request, requestToken});
            }
            return;
        }

        startWorker(request, requestToken);
    }

    void FilterPanelDistinctValueFetcher::startWorker(const domain::DistinctValuesRequest& request,
                                                      std::uint64_t requestToken) {
        activeRequestInFlight_ = true;
        activeRequestToken_ = requestToken;
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
    }

    void FilterPanelDistinctValueFetcher::onWatcherFinished() {
        if (!watcher_.isFinished()) {
            return;
        }
        // A superseded request flipped the token; drop this stale result and run
        // the pending one if any (keeps the latest user intent).
        const bool cancelled =
            activeCancelToken_ && activeCancelToken_->load(std::memory_order_relaxed);
        std::shared_ptr<std::vector<std::string>> result;
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result = activeResult_;
            activeResult_.reset();
        }
        if (!cancelled) {
            std::vector<std::string> values;
            if (result) {
                values = std::move(*result);
            }
            emit this->valuesReady(activeRequestToken_, std::move(values));
        }
        activeCancelToken_.reset();
        activeRequestInFlight_ = false;
        if (!pendingRequests_.empty()) {
            auto pending = std::move(pendingRequests_.front());
            pendingRequests_.pop_front();
            startWorker(pending.request, pending.requestToken);
        }
    }

} // namespace ssa::presentation
