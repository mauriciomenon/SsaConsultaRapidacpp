#include "presentation/BrowseOrchestrator.h"

#include <utility>

namespace ssa::presentation {

    BrowseOrchestrator::BrowseOrchestrator(std::shared_ptr<query::SsaQueryService> queryService,
                                           SearchViewModel& search, FilterPanelViewModel& filters,
                                           DetailsViewModel& details, StatusViewModel& status,
                                           SsaTableModel& tableModel, QObject* parent)
        : QObject(parent), search_(search), filters_(filters), details_(details), status_(status),
          tableModel_(tableModel), pageQueries_(std::move(queryService), this) {
        connect(&search_, &SearchViewModel::applyRequested, this, &BrowseOrchestrator::apply);
        connect(&search_, &SearchViewModel::textClearRequested, this,
                &BrowseOrchestrator::clearSearchTextAndReload);
        connect(&filters_, &FilterPanelViewModel::applyRequested, this, &BrowseOrchestrator::apply);
        connect(&pageQueries_, &PageQueryCoordinator::started, this, [this] {
            status_.setLoading(true);
            status_.setError({});
            status_.setMessage("Consultando dados...");
        });
        connect(&pageQueries_, &PageQueryCoordinator::succeeded, this,
                &BrowseOrchestrator::applyPageResult);
        connect(&pageQueries_, &PageQueryCoordinator::canceled, this,
                &BrowseOrchestrator::applyPageCanceled);
        connect(&pageQueries_, &PageQueryCoordinator::replaced, this, [this] {
            status_.setLoading(true);
            status_.setMessage("Atualizando consulta...");
        });
        connect(&pageQueries_, &PageQueryCoordinator::failed, this,
                &BrowseOrchestrator::applyPageError);
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
        const auto previousPageSize = queryState_.pageSize();
        queryState_.setPageSize(value);
        if (queryState_.pageSize() == previousPageSize) {
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

    domain::SsaPageRequest BrowseOrchestrator::currentRequest() const {
        return buildRequest();
    }

    const std::vector<std::string>& BrowseOrchestrator::visibleColumns() const {
        return queryState_.visibleColumns();
    }

    const std::map<std::string, int>& BrowseOrchestrator::columnWidths() const {
        return queryState_.columnWidths();
    }

    void BrowseOrchestrator::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        queryState_.applyPreferences(snapshot);
        tableModel_.setColumnWidths(queryState_.columnWidths());
        filters_.applyPreferences(snapshot);
        emit pageChanged();
        emit sortChanged();
    }

    void BrowseOrchestrator::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        queryState_.writePreferences(snapshot);
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
        runRequest(buildRequest());
    }

    void BrowseOrchestrator::apply() {
        queryState_.resetPage();
        emit preferencesSaveRequested();
        load();
    }

    void BrowseOrchestrator::clearSearchTextAndReload() {
        queryState_.resetPage();
        search_.setText({});
        load();
    }

    void BrowseOrchestrator::nextPage() {
        if (queryState_.nextPage()) {
            load();
        }
    }

    void BrowseOrchestrator::previousPage() {
        if (queryState_.previousPage()) {
            load();
        }
    }

    void BrowseOrchestrator::selectRow(const int row) {
        const auto record = tableModel_.recordAt(row);
        if (!record) {
            details_.clearRecord();
            return;
        }
        details_.setRecord(*record);
    }

    void BrowseOrchestrator::sortByColumn(const int column) {
        const QString key = tableModel_.columnKey(column);
        if (key.isEmpty()) {
            return;
        }
        queryState_.sortByColumnKey(key.toStdString());
        emit sortChanged();
        load();
    }

    void BrowseOrchestrator::cancelCurrentRequest() {
        pageQueries_.cancel();
        status_.setLoading(false);
        status_.setMessage("Consulta cancelada");
    }

    domain::SsaPageRequest BrowseOrchestrator::buildRequest() const {
        return queryState_.buildRequest(search_.text().toStdString(), filters_.columnFilters(),
                                        filters_.quickSector().trimmed().toStdString(),
                                        filters_.excludeScaSesSte(), filters_.advancedFilters());
    }

    void BrowseOrchestrator::runRequest(const domain::SsaPageRequest& request) {
        pageQueries_.run(request);
    }

    void BrowseOrchestrator::applyPageResult(PageQueryResult result,
                                             const domain::SsaPageRequest& request) {
        queryState_.applyPageResult(result.page);
        tableModel_.setPage(std::move(result.page), request.visibleColumns,
                            std::move(result.displayColumns), std::move(result.displayValues));
        details_.clearRecord();
        status_.setQueryComplete(queryState_.totalRows(), queryState_.pageNumber(),
                                 queryState_.pageCount());
        status_.setLoading(false);
        emit pageChanged();
    }

    void BrowseOrchestrator::applyPageError(const QString& message) {
        status_.setError(message);
        status_.setMessage("Falha ao consultar dados");
        status_.setLoading(false);
        emit pageChanged();
    }

    void BrowseOrchestrator::applyPageCanceled() {
        status_.setLoading(false);
        status_.setMessage("Consulta cancelada");
        emit pageChanged();
    }

} // namespace ssa::presentation
