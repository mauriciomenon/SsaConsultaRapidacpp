#include "presentation/MainPreferenceFlowCoordinator.h"

#include "domain/ColumnCatalog.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "query/TextFilterToken.h"

#include <QCoreApplication>
#include <QVariantMap>
#include <QtConcurrent>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {
        std::filesystem::path localFilePath(const QUrl& url) {
#ifdef Q_OS_WIN
            return std::filesystem::path{url.toLocalFile().toStdWString()};
#else
            return std::filesystem::path{url.toLocalFile().toStdString()};
#endif
        }

        QString savePresetFile(const std::shared_ptr<ports::IFilterPresetStore>& store,
                               const std::filesystem::path& path,
                               const ports::FilterPresetSnapshot& snapshot) {
            try {
                store->save(path, snapshot);
                return {};
            } catch (const std::exception& exc) {
                return QString::fromStdString(exc.what());
            }
        }

        FilterPresetLoadResult
        loadPresetFile(const std::shared_ptr<ports::IFilterPresetStore>& store,
                       const std::filesystem::path& path) {
            try {
                return {store->load(path), {}};
            } catch (const std::exception& exc) {
                return {{}, QString::fromStdString(exc.what())};
            }
        }

        bool hasFilterState(const ports::FilterPreferencesSnapshot& filters) {
            return !filters.searchText.empty() || !filters.quickSector.empty() ||
                   !filters.columnFilters.empty() || !filters.advancedTextFilters.empty() ||
                   !filters.advancedYear.empty() || !filters.advancedWeek.empty() ||
                   !filters.issueYear.empty() || !filters.executionYear.empty() ||
                   !filters.reprogrammingEquals.empty() || !filters.reprogrammingValues.empty() ||
                   !filters.issueWeekStart.empty() || !filters.issueWeekEnd.empty() ||
                   !filters.executionWeekStart.empty() || !filters.executionWeekEnd.empty() ||
                   filters.derivationMode != "all" || filters.excludeScaSesSte ||
                   filters.onlyReprogrammed;
        }

        bool sameFilters(const ports::FilterPreferencesSnapshot& lhs,
                         const ports::FilterPreferencesSnapshot& rhs) {
            return lhs.searchText == rhs.searchText && lhs.quickSector == rhs.quickSector &&
                   lhs.columnFilters == rhs.columnFilters &&
                   lhs.advancedTextFilters == rhs.advancedTextFilters &&
                   lhs.advancedWeekColumnKey == rhs.advancedWeekColumnKey &&
                   lhs.advancedYear == rhs.advancedYear && lhs.advancedWeek == rhs.advancedWeek &&
                   lhs.issueYear == rhs.issueYear && lhs.executionYear == rhs.executionYear &&
                   lhs.reprogrammingEquals == rhs.reprogrammingEquals &&
                   lhs.reprogrammingMode == rhs.reprogrammingMode &&
                   lhs.reprogrammingValues == rhs.reprogrammingValues &&
                   lhs.issueWeekStart == rhs.issueWeekStart &&
                   lhs.issueWeekEnd == rhs.issueWeekEnd &&
                   lhs.executionWeekStart == rhs.executionWeekStart &&
                   lhs.executionWeekEnd == rhs.executionWeekEnd &&
                   lhs.derivationMode == rhs.derivationMode &&
                   lhs.excludeScaSesSte == rhs.excludeScaSesSte &&
                   lhs.onlyReprogrammed == rhs.onlyReprogrammed;
        }

        QString normalizedName(const QString& name) {
            return name.trimmed();
        }

        void sortSavedFilters(std::vector<ports::SavedFilterSnapshot>& filters) {
            std::ranges::sort(filters, [](const auto& lhs, const auto& rhs) {
                return QString::fromStdString(lhs.name).localeAwareCompare(
                           QString::fromStdString(rhs.name)) < 0;
            });
        }

        bool sameSavedFilters(const std::vector<ports::SavedFilterSnapshot>& lhs,
                              const std::vector<ports::SavedFilterSnapshot>& rhs) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (std::size_t index = 0; index < lhs.size(); ++index) {
                if (lhs[index].name != rhs[index].name ||
                    !sameFilters(lhs[index].filters, rhs[index].filters)) {
                    return false;
                }
            }
            return true;
        }

        void normalizeQuickSector(ports::FilterPreferencesSnapshot& filters) {
            const auto executorKey = std::string{domain::ColumnCatalog::executorColumnKey()};
            auto executor = filters.advancedTextFilters.find(executorKey);
            if (filters.quickSector.empty() || executor == filters.advancedTextFilters.end() ||
                executor->second.empty()) {
                return;
            }
            auto tokens = query::parseTextFilterTokens(executor->second);
            query::addTextFilterValue(tokens, filters.quickSector,
                                      query::TextFilterOperator::Equals);
            executor->second = query::joinTextFilterTokens(tokens);
            filters.quickSector.clear();
        }

        void normalizeColumnOverlap(ports::FilterPreferencesSnapshot& filters) {
            for (const auto& [key, value] : filters.advancedTextFilters) {
                filters.columnFilters.erase(key);
            }
        }

        bool includesExcludedStatus(const std::map<std::string, std::string>& textFilters) {
            const auto statusKey = std::string{domain::ColumnCatalog::statusColumnKey()};
            const auto filter = textFilters.find(statusKey);
            if (filter == textFilters.end()) {
                return false;
            }
            const auto tokens = query::parseTextFilterTokens(filter->second);
            const auto excluded = domain::ColumnCatalog::excludedStatusCodes();
            return std::ranges::any_of(tokens.ordered, [excluded](const auto& token) {
                return token.filterOperator == query::TextFilterOperator::Equals &&
                       std::ranges::find(excluded, std::string_view{token.value}) != excluded.end();
            });
        }

        void normalizeStatusExclusion(ports::FilterPreferencesSnapshot& filters) {
            if (!filters.excludeScaSesSte) {
                return;
            }
            if (!includesExcludedStatus(filters.advancedTextFilters) &&
                !includesExcludedStatus(filters.columnFilters)) {
                return;
            }
            filters.excludeScaSesSte = false;
        }

        void normalizeFilters(ports::FilterPreferencesSnapshot& filters) {
            normalizeQuickSector(filters);
            normalizeColumnOverlap(filters);
            normalizeStatusExclusion(filters);
        }

        void normalizeSavedFilters(std::vector<ports::SavedFilterSnapshot>& filters) {
            for (auto& saved : filters) {
                normalizeFilters(saved.filters);
            }
        }
    } // namespace

    MainPreferenceFlowCoordinator::MainPreferenceFlowCoordinator(
        BrowseViewModel& browse, UiSettingsViewModel& ui, ColumnSettingsModel& columns,
        UserPreferencesCoordinator& preferences,
        std::shared_ptr<ports::IFilterPresetStore> presetStore,
        application::FilterPresetService& presetService, QObject* parent)
        : QObject(parent), browse_(browse), ui_(ui), columns_(columns), preferences_(preferences),
          presetStore_(std::move(presetStore)), presetService_(presetService) {
        connect(&exportPresetWatcher_, &QFutureWatcher<QString>::finished, this,
                &MainPreferenceFlowCoordinator::finishExportFilterPreset);
        connect(&importPresetWatcher_, &QFutureWatcher<FilterPresetLoadResult>::finished, this,
                &MainPreferenceFlowCoordinator::finishImportFilterPreset);
    }

    MainPreferenceFlowCoordinator::~MainPreferenceFlowCoordinator() {
        waitForPresetTasks();
    }

    ports::UserPreferencesSnapshot MainPreferenceFlowCoordinator::buildPreferencesSnapshot() const {
        ports::UserPreferencesSnapshot snapshot;
        ui_.writePreferences(snapshot);
        browse_.writePreferences(snapshot);
        snapshot.visibleColumns = columns_.visibleKeys();
        snapshot.columnWidths = columns_.columnWidths();
        snapshot.savedFilters = savedFilters_;
        normalizeFilters(snapshot.filters);
        normalizeSavedFilters(snapshot.savedFilters);
        return snapshot;
    }

    QVariantList MainPreferenceFlowCoordinator::savedFilters() const {
        QVariantList filters;
        filters.reserve(static_cast<qsizetype>(savedFilters_.size()));
        for (const auto& saved : savedFilters_) {
            QVariantMap item;
            item.insert(QStringLiteral("name"), QString::fromStdString(saved.name));
            filters.push_back(item);
        }
        return filters;
    }

    void MainPreferenceFlowCoordinator::applyStoredPreferences(
        const ports::UserPreferencesSnapshot& snapshot) {
        ui_.applyPreferences(snapshot);
        browse_.applyPreferences(snapshot);
        columns_.applyPreferences(snapshot);
        auto storedSavedFilters = snapshot.savedFilters;
        normalizeSavedFilters(storedSavedFilters);
        setSavedFilters(std::move(storedSavedFilters));
    }

    void MainPreferenceFlowCoordinator::scheduleSavePreferences() {
        preferences_.scheduleSave([this] { return buildPreferencesSnapshot(); });
    }

    void MainPreferenceFlowCoordinator::saveAppliedColumnPreferences(
        std::vector<std::string> visibleColumns, std::map<std::string, int> columnWidths) {
        auto snapshot = buildPreferencesSnapshot();
        snapshot.visibleColumns = std::move(visibleColumns);
        snapshot.columnWidths = std::move(columnWidths);
        preferences_.saveNowOrSchedule(std::move(snapshot));
    }

    void MainPreferenceFlowCoordinator::saveNowOrSchedule() {
        preferences_.saveNowOrSchedule(buildPreferencesSnapshot());
    }

    void MainPreferenceFlowCoordinator::savePreferences() {
        saveNowOrSchedule();
        emit this->statusMessageRequested("Salvamento solicitado");
    }

    QString MainPreferenceFlowCoordinator::suggestedFilterName() const {
        const auto snapshot = buildPreferencesSnapshot();
        const auto searchText = QString::fromStdString(snapshot.filters.searchText).trimmed();
        if (!searchText.isEmpty()) {
            return searchText;
        }
        if (!snapshot.filters.quickSector.empty()) {
            return QString::fromStdString(snapshot.filters.quickSector);
        }
        return QStringLiteral("Filtro combinado %1").arg(savedFilters_.size() + 1);
    }

    void MainPreferenceFlowCoordinator::saveCurrentFilter(const QString& name) {
        const auto filterName = normalizedName(name);
        if (filterName.isEmpty()) {
            emit this->statusErrorRequested("Informe um nome para salvar o filtro");
            return;
        }
        auto snapshot = buildPreferencesSnapshot();
        normalizeFilters(snapshot.filters);
        if (!hasFilterState(snapshot.filters)) {
            emit this->statusMessageRequested("Aplique algum filtro antes de salvar");
            return;
        }
        const auto nameExists = std::ranges::any_of(savedFilters_, [&filterName](const auto& item) {
            return QString::fromStdString(item.name).compare(filterName, Qt::CaseInsensitive) == 0;
        });
        if (nameExists) {
            emit this->statusMessageRequested("Filtro salvo com este nome ja existe");
            return;
        }
        const auto stateExists = std::ranges::any_of(savedFilters_, [&snapshot](const auto& item) {
            return sameFilters(item.filters, snapshot.filters);
        });
        if (stateExists) {
            emit this->statusMessageRequested("Este filtro ja esta salvo");
            return;
        }
        savedFilters_.push_back(
            ports::SavedFilterSnapshot{filterName.toStdString(), std::move(snapshot.filters)});
        sortSavedFilters(savedFilters_);
        emit savedFiltersChanged();
        saveNowOrSchedule();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtro salvo");
    }

    void MainPreferenceFlowCoordinator::applySavedFilter(const QString& name) {
        const auto filterName = normalizedName(name);
        const auto filter = std::ranges::find_if(savedFilters_, [&filterName](const auto& item) {
            return QString::fromStdString(item.name) == filterName;
        });
        if (filter == savedFilters_.end()) {
            emit this->statusErrorRequested("Filtro salvo nao encontrado");
            return;
        }
        auto snapshot = buildPreferencesSnapshot();
        snapshot.filters = filter->filters;
        normalizeFilters(snapshot.filters);
        snapshot.savedFilters = savedFilters_;
        applyStoredPreferences(snapshot);
        browse_.apply();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtro aplicado");
    }

    void MainPreferenceFlowCoordinator::removeSavedFilter(const QString& name) {
        const auto filterName = normalizedName(name);
        const auto previousSize = savedFilters_.size();
        std::erase_if(savedFilters_, [&filterName](const auto& item) {
            return QString::fromStdString(item.name) == filterName;
        });
        if (savedFilters_.size() == previousSize) {
            emit this->statusErrorRequested("Filtro salvo nao encontrado");
            return;
        }
        emit savedFiltersChanged();
        saveNowOrSchedule();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtro removido");
    }

    void MainPreferenceFlowCoordinator::exportFilterPreset(const QUrl& outputUrl) {
        if (!presetStore_) {
            emit this->statusErrorRequested("Exportacao de filtros nao configurada");
            return;
        }
        if (!outputUrl.isLocalFile()) {
            emit this->statusErrorRequested("Preset de filtros deve ser arquivo local");
            return;
        }
        if (exportPresetWatcher_.isRunning() || importPresetWatcher_.isRunning()) {
            emit this->statusErrorRequested("Operacao de filtros em andamento");
            return;
        }

        const auto store = presetStore_;
        exportPresetWatcher_.setFuture(QtConcurrent::run(
            [store, path = localFilePath(outputUrl),
             snapshot = presetService_.createPresetWithClearedSearch(buildPreferencesSnapshot())] {
                return savePresetFile(store, path, snapshot);
            }));
        emit this->statusMessageRequested("Exportando filtros");
    }

    void MainPreferenceFlowCoordinator::importFilterPreset(const QUrl& inputUrl) {
        if (!presetStore_) {
            emit this->statusErrorRequested("Importacao de filtros nao configurada");
            return;
        }
        if (!inputUrl.isLocalFile()) {
            emit this->statusErrorRequested("Preset de filtros deve ser arquivo local");
            return;
        }
        if (exportPresetWatcher_.isRunning() || importPresetWatcher_.isRunning()) {
            emit this->statusErrorRequested("Operacao de filtros em andamento");
            return;
        }

        const auto store = presetStore_;
        importPresetWatcher_.setFuture(QtConcurrent::run(
            [store, path = localFilePath(inputUrl)] { return loadPresetFile(store, path); }));
        emit this->statusMessageRequested("Importando filtros");
    }

    void MainPreferenceFlowCoordinator::finishExportFilterPreset() {
        const auto error = exportPresetWatcher_.future().result();
        if (error.isEmpty()) {
            emit this->statusErrorClearRequested();
            emit this->statusMessageRequested("Filtros exportados");
        } else {
            emit this->statusErrorRequested(error);
        }
    }

    void MainPreferenceFlowCoordinator::finishImportFilterPreset() {
        const auto result = importPresetWatcher_.future().result();
        if (!result.error.isEmpty()) {
            emit this->statusErrorRequested(result.error);
            return;
        }
        auto baseSnapshot = buildPreferencesSnapshot();
        presetService_.applyPresetPreservingSearch(result.snapshot, baseSnapshot);
        normalizeFilters(baseSnapshot.filters);
        applyStoredPreferences(baseSnapshot);
        browse_.apply();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtros importados");
    }

    void MainPreferenceFlowCoordinator::waitForPresetTasks() {
        // Wait for the workers AND pump the event loop so the queued 'finished'
        // signals are handled (and their ResultStore results consumed) before the
        // watchers' destructors run. Without processEvents the QFutureInterface<T>
        // result can race teardown (TSan: data race in ResultStore clear).
        if (exportPresetWatcher_.isRunning()) {
            exportPresetWatcher_.waitForFinished();
        }
        if (importPresetWatcher_.isRunning()) {
            importPresetWatcher_.waitForFinished();
        }
        QCoreApplication::processEvents();
    }

    void MainPreferenceFlowCoordinator::requestSaveFromSignal() {
        scheduleSavePreferences();
    }

    void MainPreferenceFlowCoordinator::setSavedFilters(
        std::vector<ports::SavedFilterSnapshot> filters) {
        sortSavedFilters(filters);
        normalizeSavedFilters(filters);
        if (sameSavedFilters(savedFilters_, filters)) {
            return;
        }
        savedFilters_ = std::move(filters);
        emit savedFiltersChanged();
    }

} // namespace ssa::presentation
