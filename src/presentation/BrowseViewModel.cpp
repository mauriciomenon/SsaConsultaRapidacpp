#include "presentation/BrowseViewModel.h"

#include "domain/ColumnCatalog.h"

#include <QVariantMap>

#include <utility>

namespace ssa::presentation {

    BrowseViewModel::BrowseViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                     QObject* parent)
        : QObject(parent), search_(this), filters_(queryService, this), details_(this), status_(this),
          tableModel_(std::string{domain::kSsaNumberColumnKey}, this),
          orchestrator_(std::move(queryService), search_, filters_, details_, status_, tableModel_,
                        this) {
        connect(&orchestrator_, &BrowseOrchestrator::pageChanged, this,
                &BrowseViewModel::pageChanged);
        connect(&orchestrator_, &BrowseOrchestrator::sortChanged, this, [this] {
            emit sortChanged();
            emit tableHeadersChanged();
        });
        connect(&filters_, &FilterPanelViewModel::changed, this,
                &BrowseViewModel::tableHeadersChanged);
        connect(&tableModel_, &SsaTableModel::columnsChanged, this,
                &BrowseViewModel::tableHeadersChanged);
        connect(&orchestrator_, &BrowseOrchestrator::preferencesSaveRequested, this,
                &BrowseViewModel::preferencesSaveRequested);
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

    QVariantList BrowseViewModel::tableHeaders() const {
        QVariantList headers;
        const auto columns = tableModel_.tableColumns();
        const auto widths = tableModel_.columnWidths();
        headers.reserve(columns.size());
        for (qsizetype index = 0; index < columns.size(); ++index) {
            QVariantMap header = columns[index].toMap();
            const auto key = header.value("key").toString();
            auto label = header.value("label").toString();
            if (filters_.hasFilterForColumn(key)) {
                label += " [f]";
            }
            if (key == sortColumnKey()) {
                label += sortAscending() ? "  ^" : "  v";
            }
            header.insert("label", label);
            header.insert("width", index < widths.size() ? widths[index].toInt() : 0);
            headers.push_back(header);
        }
        return headers;
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

    void BrowseViewModel::setFilterColumn(const QString& key) {
        filters_.setColumnKey(key);
    }

    void BrowseViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        orchestrator_.applyPreferences(snapshot);
    }

    void BrowseViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        orchestrator_.writePreferences(snapshot);
    }

    void BrowseViewModel::applyColumnSettings(std::vector<std::string> visibleColumns,
                                              std::map<std::string, int> columnWidths) {
        orchestrator_.applyColumnSettings(std::move(visibleColumns), std::move(columnWidths));
    }

    void BrowseViewModel::load() {
        orchestrator_.load();
    }

    void BrowseViewModel::apply() {
        orchestrator_.apply();
    }

    void BrowseViewModel::clearSearchTextAndReload() {
        orchestrator_.clearSearchTextAndReload();
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

    void BrowseViewModel::sortByColumn(const int column) {
        orchestrator_.sortByColumn(column);
    }

    void BrowseViewModel::cancelCurrentRequest() {
        orchestrator_.cancelCurrentRequest();
    }

} // namespace ssa::presentation
