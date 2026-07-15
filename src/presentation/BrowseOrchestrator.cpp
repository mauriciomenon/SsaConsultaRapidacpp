#include "presentation/BrowseOrchestrator.h"

#include <QStringList>

#include <algorithm>
#include <ranges>
#include <utility>

namespace ssa::presentation {
    namespace {
        constexpr std::size_t kMaxFilterHistoryDepth = 10;
    }

    BrowseOrchestrator::BrowseOrchestrator(std::shared_ptr<ports::ISsaBrowsePort> browsePort,
                                           SearchViewModel& search, FilterPanelViewModel& filters,
                                           DetailsViewModel& details, StatusViewModel& status,
                                           SsaTableModel& tableModel, QObject* parent)
        : QObject(parent), search_(search), filters_(filters), status_(status),
          tableModel_(tableModel), inputCoordinator_(queryState_, this),
          selectionCoordinator_(details, tableModel, this),
          requestCoordinator_(std::move(browsePort), queryState_, search_, filters_, status,
                              tableModel, this) {
        connect(&requestCoordinator_, &BrowseRequestCoordinator::activeOperationsChanged, this,
                &BrowseOrchestrator::activeOperationsChanged);
        connect(&search_, &SearchViewModel::applyRequested, this, &BrowseOrchestrator::apply);
        connect(&search_, &SearchViewModel::textClearRequested, this,
                &BrowseOrchestrator::clearSearchAndResetPage);
        connect(&filters_, &FilterPanelViewModel::applyRequested, this, &BrowseOrchestrator::apply);
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

    qlonglong BrowseOrchestrator::totalRowsAll() const {
        return queryState_.totalRowsAll();
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

    bool BrowseOrchestrator::canUndoFilters() const {
        return !filterHistory_.empty();
    }

    int BrowseOrchestrator::filterUndoDepth() const {
        return static_cast<int>(filterHistory_.size());
    }

    QString BrowseOrchestrator::filterHistoryText() const {
        QStringList states;
        int level = 1;
        for (const auto& state : std::views::reverse(filterHistory_)) {
            QStringList conditions;
            conditions
                << QStringLiteral("level=%1").arg(level++)
                << QStringLiteral("search=%1").arg(QString::fromStdString(state.searchText))
                << QStringLiteral("quickSector=%1").arg(QString::fromStdString(state.quickSector));
            const auto appendMap = [&conditions](const QString& name, const auto& values) {
                QStringList entries;
                for (const auto& [key, value] : values) {
                    entries << QStringLiteral("%1:%2").arg(QString::fromStdString(key),
                                                           QString::fromStdString(value));
                }
                conditions << QStringLiteral("%1=%2").arg(name, entries.join(','));
            };
            appendMap(QStringLiteral("columnFilters"), state.columnFilters);
            appendMap(QStringLiteral("advancedTextFilters"), state.advancedTextFilters);
            conditions
                << QStringLiteral("advancedWeekColumnKey=%1")
                       .arg(QString::fromStdString(state.advancedWeekColumnKey))
                << QStringLiteral("advancedYear=%1").arg(QString::fromStdString(state.advancedYear))
                << QStringLiteral("advancedWeek=%1").arg(QString::fromStdString(state.advancedWeek))
                << QStringLiteral("issueYear=%1").arg(QString::fromStdString(state.issueYear))
                << QStringLiteral("executionYear=%1")
                       .arg(QString::fromStdString(state.executionYear))
                << QStringLiteral("reprogrammingMode=%1")
                       .arg(QString::fromStdString(state.reprogrammingMode))
                << QStringLiteral("reprogrammingValues=%1")
                       .arg(QString::fromStdString(state.reprogrammingValues))
                << QStringLiteral("issueWeekStart=%1")
                       .arg(QString::fromStdString(state.issueWeekStart))
                << QStringLiteral("issueWeekEnd=%1").arg(QString::fromStdString(state.issueWeekEnd))
                << QStringLiteral("executionWeekStart=%1")
                       .arg(QString::fromStdString(state.executionWeekStart))
                << QStringLiteral("executionWeekEnd=%1")
                       .arg(QString::fromStdString(state.executionWeekEnd))
                << QStringLiteral("derivationMode=%1")
                       .arg(QString::fromStdString(state.derivationMode))
                << QStringLiteral("excludeScaSesSte=%1").arg(state.excludeScaSesSte)
                << QStringLiteral("onlyReprogrammed=%1").arg(state.onlyReprogrammed);
            states << conditions.join(QStringLiteral("; "));
        }
        return states.join('\n');
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
        const bool hadHistory = canUndoFilters();
        queryState_.applyPreferences(snapshot);
        setFilterPreferences(snapshot);
        tableModel_.setColumnWidths(queryState_.columnWidths());
        appliedFilters_ = currentFilters();
        filterHistory_.clear();
        if (hadHistory) {
            emit filterHistoryChanged();
        }
        emit pageChanged();
        emit sortChanged();
    }

    void BrowseOrchestrator::setFilterPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        search_.setText(QString::fromStdString(snapshot.filters.searchText));
        filters_.applyPreferences(snapshot);
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

    void BrowseOrchestrator::invalidateTotalRowsAll() {
        requestCoordinator_.invalidateTotalRowsAll();
    }

    bool BrowseOrchestrator::hasActiveOperations() const {
        return requestCoordinator_.hasActiveOperations();
    }

    void BrowseOrchestrator::load() {
        selectionCoordinator_.clearSelection();
        requestCoordinator_.loadCurrentRequest();
    }

    void BrowseOrchestrator::apply() {
        bool historyChanged = false;
        const auto filters = currentFilters();
        if (appliedFilters_ && filters != *appliedFilters_) {
            filterHistory_.push_back(*appliedFilters_);
            if (filterHistory_.size() > kMaxFilterHistoryDepth) {
                filterHistory_.pop_front();
            }
            historyChanged = true;
        }
        appliedFilters_ = filters;
        if (historyChanged) {
            emit filterHistoryChanged();
        }
        inputCoordinator_.apply();
        emit preferencesSaveRequested();
        load();
    }

    void BrowseOrchestrator::clearSearchAndResetPage() {
        search_.setText({});
        apply();
    }

    void BrowseOrchestrator::undoFilters() {
        undoFilterLevels(1);
    }

    void BrowseOrchestrator::undoFilterLevels(const int levels) {
        if (levels < 1 || levels > filterUndoDepth()) {
            return;
        }
        ports::UserPreferencesSnapshot snapshot;
        const auto target = filterHistory_.end() - levels;
        snapshot.filters = *target;
        filterHistory_.erase(target, filterHistory_.end());
        setFilterPreferences(snapshot);
        appliedFilters_ = currentFilters();
        inputCoordinator_.apply();
        emit filterHistoryChanged();
        emit preferencesSaveRequested();
        load();
    }

    ports::FilterPreferencesSnapshot BrowseOrchestrator::currentFilters() const {
        ports::UserPreferencesSnapshot snapshot;
        snapshot.filters.searchText = search_.text().toStdString();
        filters_.writePreferences(snapshot);
        return snapshot.filters;
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
        emit preferencesSaveRequested();
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
