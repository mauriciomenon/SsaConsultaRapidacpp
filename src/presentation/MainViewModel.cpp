#include "presentation/MainViewModel.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    MainViewModel::MainViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                 std::shared_ptr<ports::IExternalCommandPort> commandPort,
                                 std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
                                 QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)),
          preferencesStore_(std::move(preferencesStore)),
          visibleColumns_(domain::ColumnCatalog::defaultVisibleKeys()), search_(this),
          filters_(this), details_(this), status_(this), commands_(std::move(commandPort), this),
          tableModel_(this) {
        if (!queryService_) {
            throw std::invalid_argument("query service is required");
        }
        loadPreferences();
        connect(&search_, &SearchViewModel::applyRequested, this, &MainViewModel::apply);
        connect(&search_, &SearchViewModel::clearRequested, this, &MainViewModel::clearSearch);
        connect(&filters_, &FilterPanelViewModel::applyRequested, this, &MainViewModel::apply);
    }

    SearchViewModel* MainViewModel::search() {
        return &search_;
    }

    FilterPanelViewModel* MainViewModel::filters() {
        return &filters_;
    }

    DetailsViewModel* MainViewModel::details() {
        return &details_;
    }

    StatusViewModel* MainViewModel::status() {
        return &status_;
    }

    CommandViewModel* MainViewModel::commands() {
        return &commands_;
    }

    SsaTableModel* MainViewModel::tableModel() {
        return &tableModel_;
    }

    int MainViewModel::pageIndex() const {
        return static_cast<int>(pageIndex_ + 1);
    }

    int MainViewModel::pageCount() const {
        if (totalRows_ == 0) {
            return 1;
        }
        return static_cast<int>((totalRows_ + pageSize_ - 1) / pageSize_);
    }

    int MainViewModel::totalRows() const {
        return static_cast<int>(totalRows_);
    }

    int MainViewModel::pageSize() const {
        return static_cast<int>(pageSize_);
    }

    void MainViewModel::setPageSize(const int value) {
        const int bounded = std::clamp(value, 10, 500);
        const auto boundedSize = static_cast<std::size_t>(bounded);
        if (pageSize_ == boundedSize) {
            return;
        }
        pageSize_ = boundedSize;
        pageIndex_ = 0;
        savePreferences();
        load();
    }

    void MainViewModel::load() {
        runRequest(buildRequest());
    }

    void MainViewModel::apply() {
        pageIndex_ = 0;
        savePreferences();
        load();
    }

    void MainViewModel::clearSearch() {
        pageIndex_ = 0;
        load();
    }

    void MainViewModel::nextPage() {
        if (pageIndex() >= pageCount()) {
            return;
        }
        ++pageIndex_;
        load();
    }

    void MainViewModel::previousPage() {
        if (pageIndex_ == 0) {
            return;
        }
        --pageIndex_;
        load();
    }

    void MainViewModel::selectRow(const int row) {
        details_.setRecord(tableModel_.recordAt(row));
    }

    void MainViewModel::openSelectedSsa() {
        const auto selected = details_.selectedSsa();
        if (!selected.isEmpty()) {
            commands_.openSsa(selected);
        }
    }

    void MainViewModel::cancelCurrentRequest() {
        ++requestGeneration_;
        status_.setLoading(false);
        status_.setMessage("Consulta cancelada");
    }

    domain::SsaPageRequest MainViewModel::buildRequest() const {
        domain::SsaPageRequest request;
        request.pageIndex = pageIndex_;
        request.pageSize = pageSize_;
        request.searchText = search_.text().toStdString();
        request.columnFilters = filters_.columnFilters();
        request.quickSector = filters_.quickSector().trimmed().toStdString();
        request.excludeClosedStatuses = filters_.excludeClosedStatuses();
        request.visibleColumns = visibleColumns_;
        return request;
    }

    void MainViewModel::runRequest(const domain::SsaPageRequest& request) {
        const int generation = ++requestGeneration_;
        status_.setLoading(true);
        status_.setError({});
        try {
            auto result = queryService_->search(request);
            if (generation != requestGeneration_) {
                return;
            }
            totalRows_ = result.totalRows;
            pageIndex_ = result.pageIndex;
            tableModel_.setPage(std::move(result), request.visibleColumns);
            details_.setRecord(tableModel_.recordAt(0));
            status_.setMessage(QString("%1 registros, pagina %2 de %3")
                                   .arg(totalRows())
                                   .arg(pageIndex())
                                   .arg(pageCount()));
        } catch (const std::exception& exc) {
            status_.setError(QString::fromUtf8(exc.what()));
            status_.setMessage("Falha ao consultar dados");
        }
        status_.setLoading(false);
        emit pageChanged();
    }

    void MainViewModel::loadPreferences() {
        if (!preferencesStore_) {
            return;
        }
        const auto snapshot = preferencesStore_->load();
        pageSize_ = static_cast<std::size_t>(std::clamp(snapshot.pageSize, 10, 500));
        if (!snapshot.visibleColumns.empty()) {
            visibleColumns_ = snapshot.visibleColumns;
        }
        filters_.setQuickSector(QString::fromStdString(snapshot.quickSector));
        filters_.setExcludeClosedStatuses(snapshot.excludeClosedStatuses);
        filters_.setColumnFilters(snapshot.columnFilters);
    }

    void MainViewModel::savePreferences() {
        if (!preferencesStore_) {
            return;
        }
        ports::UserPreferencesSnapshot snapshot;
        snapshot.visibleColumns = visibleColumns_;
        snapshot.pageSize = static_cast<int>(pageSize_);
        snapshot.quickSector = filters_.quickSector().trimmed().toStdString();
        snapshot.excludeClosedStatuses = filters_.excludeClosedStatuses();
        snapshot.columnFilters = filters_.columnFilters();
        try {
            preferencesStore_->save(snapshot);
        } catch (const std::exception& exc) {
            status_.setError(QString::fromUtf8(exc.what()));
        }
    }

} // namespace ssa::presentation
