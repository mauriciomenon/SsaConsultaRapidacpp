#pragma once

#include "domain/SsaTypes.h"
#include "ports/ISsaBrowsePort.h"
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
#include <optional>

namespace ssa::presentation {

    class BrowseRequestCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowseRequestCoordinator(std::shared_ptr<ports::ISsaBrowsePort> browsePort,
                                          BrowseQueryState& queryState, SearchViewModel& search,
                                          FilterPanelViewModel& filters, StatusViewModel& status,
                                          SsaTableModel& tableModel, QObject* parent = nullptr);

        // The request whose page the table shows; what the user typed but did not apply
        // (search draft) is not part of it. Before the first page, the current inputs.
        [[nodiscard]] domain::SsaPageRequest displayedRequest() const;
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
        std::optional<domain::SsaPageRequest> displayedRequest_;
    };

} // namespace ssa::presentation
