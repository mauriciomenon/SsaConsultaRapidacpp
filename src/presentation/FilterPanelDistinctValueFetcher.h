#pragma once

#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <QFutureWatcher>
#include <QObject>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ssa::presentation {

    class FilterPanelDistinctValueFetcher final : public QObject {
        Q_OBJECT

      public:
        explicit FilterPanelDistinctValueFetcher(
            std::shared_ptr<query::SsaQueryService> queryService, QObject* parent = nullptr);
        ~FilterPanelDistinctValueFetcher() override;

        void requestValues(const domain::DistinctValuesRequest& request,
                           std::uint64_t requestToken);
        void clearPendingRequests();

      signals:
        void valuesReady(std::uint64_t requestToken, std::vector<std::string> values);

      private:
        void onWatcherFinished();
        void startWorker(const domain::DistinctValuesRequest& request, std::uint64_t requestToken);

        std::shared_ptr<query::SsaQueryService> queryService_;
        // void watcher: QFutureWatcher<T> for non-trivial T has a data race in
        // QFutureInterface's ResultStore when torn down while the worker reports
        // its result (TSan: race in vector<string> destruction). The result is
        // carried via a shared_ptr, guarded by resultMutex_.
        QFutureWatcher<void> watcher_;
        std::shared_ptr<std::atomic_bool> activeCancelToken_;
        std::shared_ptr<std::vector<std::string>> activeResult_;
        std::mutex resultMutex_;
        std::uint64_t activeRequestToken_{0};
        // When a request arrives while a worker is running, it is queued here and
        // dispatched when the worker finishes, instead of cancel+setFuture which
        // races the runnable vptr.
        struct PendingRequest final {
            domain::DistinctValuesRequest request;
            std::uint64_t requestToken{0};
        };

        std::deque<PendingRequest> pendingRequests_;
    };

} // namespace ssa::presentation
