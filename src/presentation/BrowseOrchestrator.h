#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseQueryState.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/PageQueryCoordinator.h"
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
        void clearSearchTextAndReload();
        void nextPage();
        void previousPage();
        void selectRow(int row);
        void sortByColumn(int column);
        void cancelCurrentRequest();

      private:
        [[nodiscard]] domain::SsaPageRequest buildRequest() const;
        void runRequest(const domain::SsaPageRequest& request);
        void applyPageResult(PageQueryResult result, const domain::SsaPageRequest& request);
        void applyPageCanceled();
        void applyPageError(const QString& message);

        SearchViewModel& search_;
        FilterPanelViewModel& filters_;
        DetailsViewModel& details_;
        StatusViewModel& status_;
        SsaTableModel& tableModel_;
        BrowseQueryState queryState_;
        PageQueryCoordinator pageQueries_;
    };

} // namespace ssa::presentation
