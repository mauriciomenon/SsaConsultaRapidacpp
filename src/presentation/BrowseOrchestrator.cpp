#include "presentation/BrowseOrchestrator.h"

#include <algorithm>
#include <utility>

namespace ssa::presentation {

    BrowseOrchestrator::BrowseOrchestrator(std::shared_ptr<query::SsaQueryService> queryService,
                                           SearchViewModel& search, FilterPanelViewModel& filters,
                                           DetailsViewModel& details, StatusViewModel& status,
                                           SsaTableModel& tableModel, QObject* parent)
        : QObject(parent), search_(search), filters_(filters), status_(status),
          tableModel_(tableModel), inputCoordinator_(queryState_, search_, this),
          selectionCoordinator_(details, tableModel, this),
          requestCoordinator_(std::move(queryService), queryState_, search_, filters_, status,
                              tableModel, this) {
        connect(&search_, &SearchViewModel::applyRequested, &inputCoordinator_,
                &BrowseInputCoordinator::apply);
        connect(&search_, &SearchViewModel::textClearRequested, &inputCoordinator_,
                &BrowseInputCoordinator::clearSearchAndResetPage);
        connect(&filters_, &FilterPanelViewModel::applyRequested, &inputCoordinator_,
                &BrowseInputCoordinator::apply);
        connect(&requestCoordinator_, &BrowseRequestCoordinator::pageChanged, this, [this] {
            if (tableModel_.rowCount() > 0) {
                const int pending = selectionCoordinator_.pendingRow();
                int target = 0;
                if (pending == -2) {
                    target = tableModel_.rowCount() - 1;
                } else if (pending >= 0) {
                    target = std::min(pending, tableModel_.rowCount() - 1);
                }
                selectionCoordinator_.consumePendingRow();
                selectionCoordinator_.selectRow(target);
            } else {
                selectionCoordinator_.consumePendingRow();
                selectionCoordinator_.clearSelection();
            }
            emit pageChanged();
        });
        connect(&selectionCoordinator_, &BrowseSelectionCoordinator::currentRowChanged, this,
                &BrowseOrchestrator::currentRowChanged);
    }

    int BrowseOrchestrator::pageNumber() const {
        return queryState_.pageNumber();
    }

    int BrowseOrchestrator::pageCount() const {
        return queryState_.pageCount();
    }

    qlonglong BrowseOrchestrator::totalRows() const {
        return queryState_.totalRows();
    }

    int BrowseOrchestrator::pageSize() const {
        return queryState_.pageSize();
    }

    void BrowseOrchestrator::setPageSize(const int value) {
        if (!inputCoordinator_.setPageSize(value)) {
            return;
        }
        emit preferencesSaveRequested();
        load();
    }

    QString BrowseOrchestrator::sortColumnKey() const {
        return queryState_.sortColumnKey();
    }

    bool BrowseOrchestrator::sortAscending() const {
        return queryState_.sortAscending();
    }

    int BrowseOrchestrator::currentRow() const {
        return selectionCoordinator_.currentRow();
    }

    bool BrowseOrchestrator::hasMorePages() const {
        return queryState_.pageNumber() < queryState_.pageCount();
    }

    bool BrowseOrchestrator::hasPreviousPages() const {
        return queryState_.pageNumber() > 1;
    }

    domain::SsaPageRequest BrowseOrchestrator::currentRequest() const {
        return requestCoordinator_.currentRequest();
    }

    const std::vector<std::string>& BrowseOrchestrator::visibleColumns() const {
        return queryState_.visibleColumns();
    }

    const std::map<std::string, int>& BrowseOrchestrator::columnWidths() const {
        return queryState_.columnWidths();
    }

    void BrowseOrchestrator::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        queryState_.applyPreferences(snapshot);
        search_.setText(QString::fromStdString(snapshot.filters.searchText));
        tableModel_.setColumnWidths(queryState_.columnWidths());
        filters_.applyPreferences(snapshot);
        emit pageChanged();
        emit sortChanged();
    }

    void BrowseOrchestrator::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        queryState_.writePreferences(snapshot);
        snapshot.filters.searchText = search_.text().toStdString();
        filters_.writePreferences(snapshot);
    }

    void BrowseOrchestrator::applyColumnSettings(std::vector<std::string> visibleColumns,
                                                 std::map<std::string, int> columnWidths) {
        const auto previousVisibleColumns = queryState_.visibleColumns();
        queryState_.applyColumnSettings(std::move(visibleColumns), std::move(columnWidths));
        tableModel_.setColumnWidths(queryState_.columnWidths());
        if (queryState_.visibleColumns() != previousVisibleColumns) {
            load();
        }
    }

    void BrowseOrchestrator::load() {
        selectionCoordinator_.clearSelection();
        requestCoordinator_.loadCurrentRequest();
    }

    void BrowseOrchestrator::apply() {
        inputCoordinator_.apply();
        emit preferencesSaveRequested();
        load();
    }

    void BrowseOrchestrator::clearSearchAndResetPage() {
        inputCoordinator_.clearSearchAndResetPage();
        load();
    }

    void BrowseOrchestrator::nextPage() {
        if (inputCoordinator_.nextPage()) {
            load();
        }
    }

    void BrowseOrchestrator::previousPage() {
        if (inputCoordinator_.previousPage()) {
            load();
        }
    }

    void BrowseOrchestrator::sortByColumn(const int column) {
        const QString key = tableModel_.columnKey(column);
        if (key.isEmpty()) {
            return;
        }
        if (!inputCoordinator_.applySortByColumn(key)) {
            return;
        }
        emit sortChanged();
        load();
    }

    void BrowseOrchestrator::cancelCurrentRequest() {
        requestCoordinator_.cancelCurrentRequest();
    }

    void BrowseOrchestrator::selectRow(const int row) {
        selectionCoordinator_.selectRow(row);
    }

    void BrowseOrchestrator::selectNextRow() {
        const int current = selectionCoordinator_.currentRow();
        if (current < 0) {
            return;
        }
        const int last = tableModel_.rowCount() - 1;
        if (current < last) {
            selectionCoordinator_.selectRow(current + 1);
            return;
        }
        if (hasMorePages()) {
            selectionCoordinator_.setPendingFirstRow();
            nextPage();
        }
    }

    void BrowseOrchestrator::selectPreviousRow() {
        const int current = selectionCoordinator_.currentRow();
        if (current < 0) {
            return;
        }
        if (current > 0) {
            selectionCoordinator_.selectRow(current - 1);
            return;
        }
        if (hasPreviousPages()) {
            selectionCoordinator_.setPendingLastRow();
            previousPage();
        }
    }

} // namespace ssa::presentation
