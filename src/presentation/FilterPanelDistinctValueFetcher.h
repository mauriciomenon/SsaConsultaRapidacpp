#pragma once

#include "domain/SsaTypes.h"
#include "ports/ISsaBrowsePort.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::presentation {

    class FilterPanelDistinctValueFetcher final : public QObject {
        Q_OBJECT

      public:
        explicit FilterPanelDistinctValueFetcher(std::shared_ptr<ports::ISsaBrowsePort> browsePort,
                                                 QObject* parent = nullptr);
        ~FilterPanelDistinctValueFetcher() override;

        void requestValues(const domain::DistinctValuesRequest& request, std::uint64_t requestToken,
                           bool measureMaxValueLength);
        void cancelRequests();
        void clearPendingRequests();
        [[nodiscard]] bool running() const;

      signals:
        void valuesReady(std::uint64_t requestToken, std::vector<std::string> values,
                         std::size_t maxValueLength);
        void valuesFailed(std::uint64_t requestToken, QString message);
        void valuesCanceled(std::uint64_t requestToken);
        void stateChanged();

      private:
        struct FetchResult final {
            std::vector<std::string> values;
            std::size_t maxValueLength{0};
            std::exception_ptr error;
            bool canceled{false};
        };

        void onWatcherFinished();
        void startWorker(const domain::DistinctValuesRequest& request, std::uint64_t requestToken,
                         bool measureMaxValueLength);

        std::shared_ptr<ports::ISsaBrowsePort> browsePort_;
        // void watcher: QFutureWatcher<T> for non-trivial T has a data race in
        // QFutureInterface's ResultStore when torn down while the worker reports
        // its result (TSan: race in vector<string> destruction). The result is
        // carried via a shared_ptr, guarded by resultMutex_.
        QFutureWatcher<void> watcher_;
        std::stop_source activeStopSource_;
        std::shared_ptr<std::atomic_bool> activeCancelToken_;
        std::shared_ptr<FetchResult> activeResult_;
        std::mutex resultMutex_;
        std::uint64_t activeRequestToken_{0};
        bool activeRequestInFlight_{false};
        // When a request arrives while a worker is running, it is queued here and
        // dispatched when the worker finishes, instead of cancel+setFuture which
        // races the runnable vptr.
        struct PendingRequest final {
            domain::DistinctValuesRequest request;
            std::uint64_t requestToken{0};
            bool measureMaxValueLength{false};
        };

        std::deque<PendingRequest> pendingRequests_;
    };

} // namespace ssa::presentation
