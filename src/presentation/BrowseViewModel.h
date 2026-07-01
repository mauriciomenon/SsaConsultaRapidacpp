#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseOrchestrator.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SearchViewModel.h"
#include "presentation/SsaTableModel.h"
#include "presentation/StatusViewModel.h"
#include "query/SsaQueryService.h"

#include <QObject>
#include <QVariantList>

#include <map>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class BrowseViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(SearchViewModel* search READ search CONSTANT)
        Q_PROPERTY(FilterPanelViewModel* filters READ filters CONSTANT)
        Q_PROPERTY(DetailsViewModel* details READ details CONSTANT)
        Q_PROPERTY(StatusViewModel* status READ status CONSTANT)
        Q_PROPERTY(SsaTableModel* tableModel READ tableModel CONSTANT)
        Q_PROPERTY(int pageNumber READ pageNumber NOTIFY pageChanged)
        Q_PROPERTY(int pageCount READ pageCount NOTIFY pageChanged)
        Q_PROPERTY(QString pageSummary READ pageSummary NOTIFY pageChanged)
        Q_PROPERTY(qlonglong totalRows READ totalRows NOTIFY pageChanged)
        Q_PROPERTY(qlonglong totalRowsAll READ totalRowsAll NOTIFY pageChanged)
        Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageChanged)
        Q_PROPERTY(QString sortColumnKey READ sortColumnKey NOTIFY sortChanged)
        Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
        Q_PROPERTY(int currentRow READ currentRow NOTIFY currentRowChanged)
        Q_PROPERTY(bool canSelectNextRow READ canSelectNextRow NOTIFY currentRowChanged)
        Q_PROPERTY(bool canSelectPreviousRow READ canSelectPreviousRow NOTIFY currentRowChanged)
        Q_PROPERTY(QVariantList tableHeaders READ tableHeaders NOTIFY tableHeadersChanged)

      public:
        explicit BrowseViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                 QObject* parent = nullptr);

        [[nodiscard]] SearchViewModel* search();
        [[nodiscard]] FilterPanelViewModel* filters();
        [[nodiscard]] DetailsViewModel* details();
        [[nodiscard]] StatusViewModel* status();
        [[nodiscard]] SsaTableModel* tableModel();
        [[nodiscard]] int pageNumber() const;
        [[nodiscard]] int pageCount() const;
        [[nodiscard]] QString pageSummary() const;
        [[nodiscard]] qlonglong totalRows() const;
        [[nodiscard]] qlonglong totalRowsAll() const;
        [[nodiscard]] int pageSize() const;
        void setPageSize(int value);
        [[nodiscard]] QString sortColumnKey() const;
        [[nodiscard]] bool sortAscending() const;
        [[nodiscard]] int currentRow() const;
        [[nodiscard]] bool canSelectNextRow() const;
        [[nodiscard]] bool canSelectPreviousRow() const;
        [[nodiscard]] QVariantList tableHeaders() const;
        [[nodiscard]] domain::SsaPageRequest currentRequest() const;
        [[nodiscard]] const std::vector<std::string>& visibleColumns() const;
        [[nodiscard]] const std::map<std::string, int>& columnWidths() const;
        Q_INVOKABLE void setFilterPanelFocusColumn(const QString& key);
        // Loads a single SSA by number into the details panel (used when
        // navigating to a related/derived SSA from the details view).
        Q_INVOKABLE bool loadDetailsBySsaNumber(const QString& ssaNumber);

        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;
        void applyColumnSettings(std::vector<std::string> visibleColumns,
                                 std::map<std::string, int> columnWidths);
        void invalidateTotalRowsAll();

      signals:
        void pageChanged();
        void sortChanged();
        void currentRowChanged(int row);
        void tableHeadersChanged();
        void preferencesSaveRequested();

      public slots:
        void load();
        void apply();
        void clearSearchAndResetPage();
        void nextPage();
        void previousPage();
        void selectRow(int row);
        void selectNextRow();
        void selectPreviousRow();
        void sortByColumn(int column);
        void cancelCurrentRequest();

      private:
        void invalidateTableHeaders() const;
        void rebuildTableHeaders() const;

        SearchViewModel search_;
        FilterPanelViewModel filters_;
        DetailsViewModel details_;
        StatusViewModel status_;
        SsaTableModel tableModel_;
        BrowseOrchestrator orchestrator_;
        mutable QVariantList cachedTableHeaders_;
        mutable bool tableHeadersDirty_{true};
    };

} // namespace ssa::presentation
