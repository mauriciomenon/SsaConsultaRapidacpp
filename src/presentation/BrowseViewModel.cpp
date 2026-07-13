#include "presentation/BrowseViewModel.h"

#include "domain/ColumnCatalog.h"

#include <QVariantMap>

#include <utility>

namespace ssa::presentation {

    BrowseViewModel::BrowseViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                     QObject* parent)
        : QObject(parent), search_(this), filters_(queryService, this),
          details_(queryService, this), status_(this),
          tableModel_(std::string{domain::kSsaNumberColumnKey}, this),
          orchestrator_(queryService, search_, filters_, details_, status_, tableModel_, this),
          queryService_(std::move(queryService)) {
        connect(&orchestrator_, &BrowseOrchestrator::pageChanged, this, [this] {
            invalidateTableHeaders();
            emit pageChanged();
        });
        connect(&orchestrator_, &BrowseOrchestrator::sortChanged, this, [this] {
            invalidateTableHeaders();
            emit sortChanged();
            emit tableHeadersChanged();
        });
        connect(&orchestrator_, &BrowseOrchestrator::currentRowChanged, this,
                &BrowseViewModel::currentRowChanged);
        connect(&filters_, &FilterPanelViewModel::changed, this, [this] {
            invalidateTableHeaders();
            emit tableHeadersChanged();
        });
        connect(&tableModel_, &SsaTableModel::columnsChanged, this, [this] {
            invalidateTableHeaders();
            emit tableHeadersChanged();
        });
        connect(&orchestrator_, &BrowseOrchestrator::preferencesSaveRequested, this,
                &BrowseViewModel::preferencesSaveRequested);
        connect(&orchestrator_, &BrowseOrchestrator::filterHistoryChanged, this,
                &BrowseViewModel::filterHistoryChanged);
    }

    SearchViewModel* BrowseViewModel::search() {
        return &search_;
    }

    FilterPanelViewModel* BrowseViewModel::filters() {
        return &filters_;
    }

    DetailsViewModel* BrowseViewModel::details() {
        return &details_;
    }

    StatusViewModel* BrowseViewModel::status() {
        return &status_;
    }

    SsaTableModel* BrowseViewModel::tableModel() {
        return &tableModel_;
    }

    int BrowseViewModel::pageNumber() const {
        return orchestrator_.pageNumber();
    }

    int BrowseViewModel::pageCount() const {
        return orchestrator_.pageCount();
    }

    qlonglong BrowseViewModel::totalRows() const {
        return orchestrator_.totalRows();
    }

    qlonglong BrowseViewModel::totalRowsAll() const {
        return orchestrator_.totalRowsAll();
    }

    int BrowseViewModel::pageSize() const {
        return orchestrator_.pageSize();
    }

    void BrowseViewModel::setPageSize(const int value) {
        orchestrator_.setPageSize(value);
    }

    QString BrowseViewModel::sortColumnKey() const {
        return orchestrator_.sortColumnKey();
    }

    bool BrowseViewModel::sortAscending() const {
        return orchestrator_.sortAscending();
    }

    int BrowseViewModel::currentRow() const {
        return orchestrator_.currentRow();
    }

    bool BrowseViewModel::canSelectNextRow() const {
        const int row = orchestrator_.currentRow();
        if (row < 0) {
            return false;
        }
        if (row < tableModel_.rowCount() - 1) {
            return true;
        }
        return orchestrator_.hasMorePages();
    }

    bool BrowseViewModel::canSelectPreviousRow() const {
        const int row = orchestrator_.currentRow();
        if (row > 0) {
            return true;
        }
        if (row < 0) {
            return false;
        }
        return orchestrator_.hasPreviousPages();
    }

    bool BrowseViewModel::canUndoFilters() const {
        return orchestrator_.canUndoFilters();
    }

    int BrowseViewModel::filterUndoDepth() const {
        return orchestrator_.filterUndoDepth();
    }

    QString BrowseViewModel::filterHistoryText() const {
        return orchestrator_.filterHistoryText();
    }

    QVariantList BrowseViewModel::tableHeaders() const {
        if (!tableHeadersDirty_) {
            return cachedTableHeaders_;
        }
        rebuildTableHeaders();
        return cachedTableHeaders_;
    }

    void BrowseViewModel::rebuildTableHeaders() const {
        const auto columns = tableModel_.tableColumns();
        const auto widths = tableModel_.columnWidths();
        QVariantList headers;
        headers.reserve(columns.size());
        const auto currentSortColumn = sortColumnKey();
        const auto isSortAscending = sortAscending();
        static const auto ssaNumberKey = QString::fromUtf8(domain::kSsaNumberColumnKey.data(),
                                                           domain::kSsaNumberColumnKey.size());

        for (qsizetype index = 0; index < columns.size(); ++index) {
            QVariantMap header = columns[index].toMap();
            const auto key = header.value("key").toString();
            header.insert("filtered", filters_.hasFilterForColumn(key));
            header.insert("sorted", key == currentSortColumn);
            header.insert("sortAscending", isSortAscending);
            header.insert("opensSam", key == ssaNumberKey);
            header.insert("width", index < widths.size() ? widths[index].toInt() : 0);
            headers.push_back(header);
        }
        cachedTableHeaders_.swap(headers);
        tableHeadersDirty_ = false;
    }

    void BrowseViewModel::invalidateTableHeaders() const {
        cachedTableHeaders_.clear();
        tableHeadersDirty_ = true;
    }

    domain::SsaPageRequest BrowseViewModel::currentRequest() const {
        return orchestrator_.currentRequest();
    }

    const std::vector<std::string>& BrowseViewModel::visibleColumns() const {
        return orchestrator_.visibleColumns();
    }

    const std::map<std::string, int>& BrowseViewModel::columnWidths() const {
        return orchestrator_.columnWidths();
    }

    void BrowseViewModel::setFilterPanelFocusColumn(const QString& key) {
        filters_.requestColumnFocus(key);
    }

    DetailsViewModel* BrowseViewModel::createDetailsWindowModel(const QString& ssaNumber,
                                                                QObject* parent) {
        auto* model = new DetailsViewModel(queryService_, parent);
        if (!ssaNumber.trimmed().isEmpty()) {
            model->requestLoadBySsaNumber(ssaNumber);
        }
        return model;
    }

    void BrowseViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        orchestrator_.applyPreferences(snapshot);
    }

    void BrowseViewModel::setFilterPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        orchestrator_.setFilterPreferences(snapshot);
    }

    void BrowseViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        orchestrator_.writePreferences(snapshot);
    }

    void BrowseViewModel::applyColumnSettings(std::vector<std::string> visibleColumns,
                                              std::map<std::string, int> columnWidths) {
        orchestrator_.applyColumnSettings(std::move(visibleColumns), std::move(columnWidths));
    }

    void BrowseViewModel::invalidateTotalRowsAll() {
        orchestrator_.invalidateTotalRowsAll();
    }

    void BrowseViewModel::load() {
        orchestrator_.load();
    }

    void BrowseViewModel::apply() {
        orchestrator_.apply();
    }

    void BrowseViewModel::clearSearchAndResetPage() {
        orchestrator_.clearSearchAndResetPage();
    }

    void BrowseViewModel::undoFilters() {
        orchestrator_.undoFilters();
    }

    void BrowseViewModel::undoFilterLevels(const int levels) {
        orchestrator_.undoFilterLevels(levels);
    }

    void BrowseViewModel::nextPage() {
        orchestrator_.nextPage();
    }

    void BrowseViewModel::previousPage() {
        orchestrator_.previousPage();
    }

    void BrowseViewModel::selectRow(const int row) {
        orchestrator_.selectRow(row);
    }

    void BrowseViewModel::selectNextRow() {
        orchestrator_.selectNextRow();
    }

    void BrowseViewModel::selectPreviousRow() {
        orchestrator_.selectPreviousRow();
    }

    void BrowseViewModel::sortByColumn(const int column) {
        orchestrator_.sortByColumn(column);
    }

    void BrowseViewModel::cancelCurrentRequest() {
        orchestrator_.cancelCurrentRequest();
    }

} // namespace ssa::presentation
