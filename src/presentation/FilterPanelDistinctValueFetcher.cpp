#include "presentation/FilterPanelDistinctValueFetcher.h"

#include <QDebug>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <memory>
#include <system_error>
#include <utility>

namespace ssa::presentation {

    FilterPanelDistinctValueFetcher::FilterPanelDistinctValueFetcher(
        std::shared_ptr<query::SsaQueryService> queryService, QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)) {
        connect(&watcher_, &QFutureWatcher<void>::finished, this, [this] { onWatcherFinished(); });
    }

    FilterPanelDistinctValueFetcher::~FilterPanelDistinctValueFetcher() {
        disconnect(&watcher_, nullptr, this, nullptr);
        clearPendingRequests();
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
        }
    }

    void FilterPanelDistinctValueFetcher::clearPendingRequests() {
        pendingRequests_.clear();
        if (activeCancelToken_) {
            activeCancelToken_->store(true, std::memory_order_relaxed);
        }
        activeStopSource_.request_stop();
    }

    void
    FilterPanelDistinctValueFetcher::requestValues(const domain::DistinctValuesRequest& request,
                                                   const std::uint64_t requestToken,
                                                   const bool measureMaxValueLength) {
        if (!queryService_) {
            emit this->valuesReady(requestToken, {}, 0);
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
                *found = PendingRequest{request, requestToken, measureMaxValueLength};
            } else {
                pendingRequests_.push_back(
                    PendingRequest{request, requestToken, measureMaxValueLength});
            }
            return;
        }

        startWorker(request, requestToken, measureMaxValueLength);
    }

    void FilterPanelDistinctValueFetcher::startWorker(const domain::DistinctValuesRequest& request,
                                                      const std::uint64_t requestToken,
                                                      const bool measureMaxValueLength) {
        activeRequestInFlight_ = true;
        activeRequestToken_ = requestToken;
        activeStopSource_ = std::stop_source{};
        activeCancelToken_ = std::make_shared<std::atomic_bool>(false);
        const auto stopToken = activeStopSource_.get_token();
        const auto cancelToken = activeCancelToken_;
        const auto service = queryService_;
        auto requestCopy = request;
        // Result is carried in a shared_ptr captured by value; the worker writes
        // through it and onWatcherFinished (owner thread) moves it out. The future
        // is void, so QFutureInterface<T>'s ResultStore (the original TSan race)
        // is never instantiated.
        const auto result = std::make_shared<FetchResult>();
        std::scoped_lock lock(resultMutex_);
        activeResult_ = result;

        watcher_.setFuture(QtConcurrent::run([service, requestCopy = std::move(requestCopy),
                                              stopToken, cancelToken, result,
                                              measureMaxValueLength] {
            // The query already projects TRIM(COALESCE(col,'')) and filters empties,
            // so the values arrive normalized; just propagate them unless cancelled.
            if (cancelToken->load(std::memory_order_relaxed)) {
                result->canceled = true;
                return;
            }
            try {
                result->values = service->distinctValues(requestCopy, stopToken);
                if (cancelToken->load(std::memory_order_relaxed) || stopToken.stop_requested()) {
                    result->canceled = true;
                    return;
                }
                if (measureMaxValueLength) {
                    result->maxValueLength =
                        service->maxValueLength(requestCopy.columnKey, stopToken);
                }
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    result->canceled = true;
                } else {
                    result->error = std::current_exception();
                }
            } catch (...) {
                result->error = std::current_exception();
            }
        }));
    }

    void FilterPanelDistinctValueFetcher::onWatcherFinished() {
        if (!watcher_.isFinished()) {
            return;
        }
        // A superseded request flipped the token; drop this stale result and run
        // the pending one if any (keeps the latest user intent).
        const bool cancelled =
            (activeCancelToken_ && activeCancelToken_->load(std::memory_order_relaxed));
        std::shared_ptr<FetchResult> result;
        {
            std::scoped_lock lock(resultMutex_);
            result = activeResult_;
            activeResult_.reset();
        }
        if (!cancelled && !(result && result->canceled)) {
            std::vector<std::string> values;
            std::size_t maxValueLength = 0;
            if (result) {
                if (result->error) {
                    try {
                        std::rethrow_exception(result->error);
                    } catch (const std::exception& error) {
                        qWarning().noquote() << "Column value query failed:" << error.what();
                    } catch (...) {
                        qWarning() << "Column value query failed: unknown error";
                    }
                } else {
                    values = std::move(result->values);
                    maxValueLength = result->maxValueLength;
                }
            }
            emit this->valuesReady(activeRequestToken_, std::move(values), maxValueLength);
        }
        activeCancelToken_.reset();
        activeRequestInFlight_ = false;
        if (!pendingRequests_.empty()) {
            auto pending = std::move(pendingRequests_.front());
            pendingRequests_.pop_front();
            startWorker(pending.request, pending.requestToken, pending.measureMaxValueLength);
        }
    }

} // namespace ssa::presentation
