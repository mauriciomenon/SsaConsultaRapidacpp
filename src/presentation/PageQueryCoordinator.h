#pragma once

#include "domain/SsaTypes.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableDisplayCache.h"
#include "query/SsaQueryService.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <optional>
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

    class PageQueryCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit PageQueryCoordinator(std::shared_ptr<query::SsaQueryService> queryService,
                                      QObject* parent = nullptr);
        ~PageQueryCoordinator() override;

        void run(domain::SsaPageRequest request);
        void cancel();

      signals:
        void started();
        void succeeded(PageQueryResult result, domain::SsaPageRequest request);
        void canceled();
        // Terminal state for a request superseded by a newer latest-wins request.
        // Explicit user cancellation is reported through canceled().
        void replaced();
        void failed(QString message);

      private:
        void start(domain::SsaPageRequest request);
        void finishActiveRequest();

        std::shared_ptr<query::SsaQueryService> queryService_;
        SsaColumnDisplayCatalog columnCatalog_;
        QFutureWatcher<PageQueryResult> watcher_;
        std::shared_ptr<std::atomic_bool> activeCancelToken_;
        std::optional<domain::SsaPageRequest> activeRequest_;
        std::optional<domain::SsaPageRequest> pendingRequest_;
        bool requestRunning_{false};
        bool activeCanceled_{false};
        bool explicitCancelRequested_{false};
        bool finishing_{false};
        bool totalRowsAllKnown_{false};
        std::size_t totalRowsAll_{0};
    };

} // namespace ssa::presentation
