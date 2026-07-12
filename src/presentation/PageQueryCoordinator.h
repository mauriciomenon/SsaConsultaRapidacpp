#pragma once

#include "domain/SsaTypes.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableDisplayCache.h"
#include "query/SsaQueryService.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::presentation {

    struct PageQueryResult {
        domain::SsaPageResult page;
        std::size_t totalRowsAll{0};
        // True when the totalRowsAll value was actually computed (vs. inherited
        // from cache / skipped). Lets the coordinator cache the grand total even
        // for superseded requests, avoiding a re-COUNT on every keystroke.
        bool totalRowsAllComputed{false};
        std::vector<SsaDisplayColumn> displayColumns;
        SsaTableDisplayValues displayValues;
        bool canceled{false};
    };

    struct PageQueryResultState {
        std::mutex mutex;
        std::optional<PageQueryResult> result;
        std::exception_ptr error;
    };

    class PageQueryCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit PageQueryCoordinator(std::shared_ptr<query::SsaQueryService> queryService,
                                      QObject* parent = nullptr);
        ~PageQueryCoordinator() override;

        void run(domain::SsaPageRequest request);
        void cancel();
        void invalidateTotalRowsAll();

      signals:
        void started();
        void succeeded(ssa::presentation::PageQueryResult result,
                       ssa::domain::SsaPageRequest request);
        void canceled();
        // Terminal state for a request superseded by a newer latest-wins request.
        // Explicit user cancellation is reported through canceled().
        void replaced();
        void failed(QString message);

      private:
        struct Operation final {
            std::uint64_t id{0};
            domain::SsaPageRequest request;
            QFutureWatcher<void> watcher;
            std::shared_ptr<PageQueryResultState> resultState;
            std::stop_source stopSource;
            std::shared_ptr<std::atomic_bool> cancelToken;
            bool explicitlyCanceled{false};
            bool completed{false};
        };

        void start(domain::SsaPageRequest request);
        void finishOperation(std::uint64_t operationId);
        void stopOperation(Operation& operation);
        void pruneCompletedOperations();
        [[nodiscard]] Operation* latestOperation();

        std::shared_ptr<query::SsaQueryService> queryService_;
        SsaColumnDisplayCatalog columnCatalog_;
        std::vector<std::unique_ptr<Operation>> operations_;
        std::uint64_t latestOperationId_{0};
        std::uint64_t nextOperationId_{0};
        bool shuttingDown_{false};
        bool totalRowsAllKnown_{false};
        std::size_t totalRowsAll_{0};
    };

} // namespace ssa::presentation
