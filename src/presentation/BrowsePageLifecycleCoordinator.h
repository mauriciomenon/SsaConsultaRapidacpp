#pragma once

#include "domain/SsaTypes.h"
#include "presentation/BrowseQueryState.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/SsaTableModel.h"
#include "presentation/StatusViewModel.h"

#include <QObject>

namespace ssa::presentation {

    class BrowsePageLifecycleCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowsePageLifecycleCoordinator(BrowseQueryState& queryState,
                                                SsaTableModel& tableModel, StatusViewModel& status,
                                                QObject* parent = nullptr);

        void markRequestStarted();
        void markRequestCanceling();
        void markRequestReplaced();
        void markRequestSucceeded(PageQueryResult result, const domain::SsaPageRequest& request);
        void markRequestCanceled();
        void markRequestFailed(const QString& message);

      private:
        BrowseQueryState& queryState_;
        SsaTableModel& tableModel_;
        StatusViewModel& status_;
    };

} // namespace ssa::presentation
