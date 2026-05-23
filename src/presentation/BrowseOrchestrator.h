#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseInputCoordinator.h"
#include "presentation/BrowseQueryState.h"
#include "presentation/BrowseRequestCoordinator.h"
#include "presentation/BrowseSelectionCoordinator.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SearchViewModel.h"
#include "presentation/SsaTableModel.h"
#include "presentation/StatusViewModel.h"
#include "query/SsaQueryService.h"

#include <QObject>

#include <map>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class BrowseOrchestrator final : public QObject {
        Q_OBJECT

      public:
        BrowseOrchestrator(std::shared_ptr<query::SsaQueryService> queryService,
                           SearchViewModel& search, FilterPanelViewModel& filters,
                           DetailsViewModel& details, StatusViewModel& status,
                           SsaTableModel& tableModel, QObject* parent = nullptr);

        [[nodiscard]] int pageNumber() const;
        [[nodiscard]] int pageCount() const;
        [[nodiscard]] qlonglong totalRows() const;
        [[nodiscard]] int pageSize() const;
        void setPageSize(int value);
        [[nodiscard]] QString sortColumnKey() const;
        [[nodiscard]] bool sortAscending() const;
        [[nodiscard]] domain::SsaPageRequest currentRequest() const;
        [[nodiscard]] const std::vector<std::string>& visibleColumns() const;
        [[nodiscard]] const std::map<std::string, int>& columnWidths() const;

        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;
        void applyColumnSettings(std::vector<std::string> visibleColumns,
                                 std::map<std::string, int> columnWidths);

      signals:
        void pageChanged();
        void sortChanged();
        void preferencesSaveRequested();

      public slots:
        void load();
        void apply();
        void clearSearchAndResetPage();
        void nextPage();
        void previousPage();
        void selectRow(int row);
        void sortByColumn(int column);
        void cancelCurrentRequest();

      private:
        SearchViewModel& search_;
        FilterPanelViewModel& filters_;
        StatusViewModel& status_;
        SsaTableModel& tableModel_;
        BrowseQueryState queryState_;
        BrowseInputCoordinator inputCoordinator_;
        BrowseSelectionCoordinator selectionCoordinator_;
        BrowseRequestCoordinator requestCoordinator_;
    };

} // namespace ssa::presentation
