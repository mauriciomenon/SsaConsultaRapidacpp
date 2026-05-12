#include "presentation/MainViewModel.h"

#include "domain/ColumnCatalog.h"

#include <QtConcurrent>

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
          columns_(this), tableModel_(this) {
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

    ColumnSettingsModel* MainViewModel::columns() {
        return &columns_;
    }

    SsaTableModel* MainViewModel::tableModel() {
        return &tableModel_;
    }

    QString MainViewModel::theme() const {
        return theme_;
    }

    void MainViewModel::setTheme(const QString& value) {
        if (theme_ == value) {
            return;
        }
        theme_ = value;
        savePreferences();
        emit preferencesChanged();
    }

    QString MainViewModel::density() const {
        return density_;
    }

    void MainViewModel::setDensity(const QString& value) {
        if (value != "compact" && value != "normal" && value != "comfortable") {
            return;
        }
        if (density_ == value) {
            return;
        }
        density_ = value;
        savePreferences();
        emit preferencesChanged();
    }

    bool MainViewModel::detailsVisible() const {
        return detailsVisible_;
    }

    void MainViewModel::setDetailsVisible(const bool value) {
        if (detailsVisible_ == value) {
            return;
        }
        detailsVisible_ = value;
        savePreferences();
        emit preferencesChanged();
    }

    int MainViewModel::pageNumber() const {
        if (totalRows_ == 0) {
            return 0;
        }
        return static_cast<int>(pageIndex_ + 1);
    }

    int MainViewModel::pageCount() const {
        if (totalRows_ == 0) {
            return 0;
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

    QString MainViewModel::sortColumnKey() const {
        return QString::fromStdString(sort_.columnKey);
    }

    bool MainViewModel::sortAscending() const {
        return sort_.ascending;
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
        search_.setText({});
        load();
    }

    void MainViewModel::nextPage() {
        const auto pages = static_cast<std::size_t>(pageCount());
        if (pages == 0 || pageIndex_ + 1 >= pages) {
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
        if (row < 0 || row >= tableModel_.rowCount()) {
            details_.setRecord(nullptr);
            return;
        }
        details_.setRecord(tableModel_.recordAt(row));
    }

    void MainViewModel::sortByColumn(const int column) {
        const QString key = tableModel_.columnKey(column);
        if (key.isEmpty()) {
            return;
        }
        const auto nextKey = key.toStdString();
        if (sort_.columnKey == nextKey) {
            sort_.ascending = !sort_.ascending;
        } else {
            sort_.columnKey = nextKey;
            sort_.ascending = true;
        }
        sort_.statusLast = sort_.columnKey == "numero_ssa";
        pageIndex_ = 0;
        emit sortChanged();
        load();
    }

    void MainViewModel::applyColumnSettings() {
        const auto previousVisibleColumns = visibleColumns_;
        visibleColumns_ = columns_.visibleKeys();
        columnWidths_ = columns_.columnWidths();
        tableModel_.setColumnWidths(columnWidths_);
        pageIndex_ = 0;
        savePreferences();
        if (visibleColumns_ != previousVisibleColumns) {
            load();
        }
    }

    void MainViewModel::resetColumnSettings() {
        columns_.resetDefaults();
        applyColumnSettings();
    }

    void MainViewModel::discardColumnSettings() {
        columns_.applyPreferences(visibleColumns_, columnWidths_);
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
        request.excludeScaSesSte = filters_.excludeScaSesSte();
        request.sort = sort_;
        request.visibleColumns = visibleColumns_;
        return request;
    }

    void MainViewModel::runRequest(const domain::SsaPageRequest& request) {
        const int generation = ++requestGeneration_;
        status_.setLoading(true);
        status_.setError({});
        status_.setMessage("Consultando dados...");
        auto* watcher = new QFutureWatcher<domain::SsaPageResult>(this);
        connect(watcher, &QFutureWatcher<domain::SsaPageResult>::finished, this,
                [this, watcher, request, generation] {
                    try {
                        auto result = watcher->result();
                        watcher->deleteLater();
                        if (generation != requestGeneration_) {
                            return;
                        }
                        totalRows_ = result.totalRows;
                        pageIndex_ = result.pageIndex;
                        tableModel_.setPage(std::move(result), request.visibleColumns);
                        if (tableModel_.rowCount() > 0) {
                            details_.setRecord(tableModel_.recordAt(0));
                        } else {
                            details_.setRecord(nullptr);
                        }
                        status_.setMessage(QString("%1 registros, pagina %2 de %3")
                                               .arg(totalRows())
                                               .arg(pageNumber())
                                               .arg(pageCount()));
                    } catch (const std::exception& exc) {
                        watcher->deleteLater();
                        if (generation != requestGeneration_) {
                            return;
                        }
                        status_.setError(QString::fromUtf8(exc.what()));
                        status_.setMessage("Falha ao consultar dados");
                    }
                    status_.setLoading(false);
                    emit pageChanged();
                });
        watcher->setFuture(QtConcurrent::run(
            [service = queryService_, request] { return service->search(request); }));
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
        theme_ = QString::fromStdString(snapshot.theme);
        const QString density = QString::fromStdString(snapshot.density);
        if (density == "compact" || density == "normal" || density == "comfortable") {
            density_ = density;
        }
        detailsVisible_ = snapshot.detailsVisible;
        columns_.applyPreferences(visibleColumns_, snapshot.columnWidths);
        visibleColumns_ = columns_.visibleKeys();
        columnWidths_ = columns_.columnWidths();
        tableModel_.setColumnWidths(columnWidths_);
        filters_.setQuickSector(QString::fromStdString(snapshot.quickSector));
        filters_.setExcludeScaSesSte(snapshot.excludeScaSesSte);
        filters_.setColumnFilters(snapshot.columnFilters);
    }

    void MainViewModel::savePreferences() {
        if (!preferencesStore_) {
            return;
        }
        ports::UserPreferencesSnapshot snapshot;
        snapshot.visibleColumns = visibleColumns_;
        snapshot.columnWidths = columnWidths_;
        snapshot.pageSize = static_cast<int>(pageSize_);
        snapshot.theme = theme_.toStdString();
        snapshot.density = density_.toStdString();
        snapshot.detailsVisible = detailsVisible_;
        snapshot.quickSector = filters_.quickSector().trimmed().toStdString();
        snapshot.excludeScaSesSte = filters_.excludeScaSesSte();
        snapshot.columnFilters = filters_.columnFilters();
        try {
            preferencesStore_->save(snapshot);
        } catch (const std::exception& exc) {
            status_.setError(QString::fromUtf8(exc.what()));
        }
    }

} // namespace ssa::presentation
