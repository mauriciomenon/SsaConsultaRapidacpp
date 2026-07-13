#pragma once

#include "domain/SsaTypes.h"
#include "presentation/BrowsePageLifecycleCoordinator.h"
#include "presentation/BrowseQueryState.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/SearchViewModel.h"
#include "presentation/SsaTableModel.h"
#include "presentation/StatusViewModel.h"

#include <QObject>
#include <QString>
#include <memory>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class BrowseRequestCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowseRequestCoordinator(std::shared_ptr<query::SsaQueryService> queryService,
                                          BrowseQueryState& queryState, SearchViewModel& search,
                                          FilterPanelViewModel& filters, StatusViewModel& status,
                                          SsaTableModel& tableModel, QObject* parent = nullptr);

        [[nodiscard]] domain::SsaPageRequest currentRequest() const;
        void loadCurrentRequest();
        void cancelCurrentRequest();
        void invalidateTotalRowsAll();
        [[nodiscard]] bool hasActiveOperations() const;

      signals:
        void pageChanged();
        void activeOperationsChanged();

      private:
        [[nodiscard]] domain::SsaPageRequest buildRequest() const;
        void runRequest(const domain::SsaPageRequest& request);

        BrowseQueryState& queryState_;
        SearchViewModel& search_;
        FilterPanelViewModel& filters_;
        SsaTableModel& tableModel_;
        BrowsePageLifecycleCoordinator pageLifecycle_;
        PageQueryCoordinator pageQueries_;
    };

} // namespace ssa::presentation
