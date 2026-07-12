#include "presentation/MainPreferenceFlowCoordinator.h"

#include "presentation/FilterPreferencesNormalizer.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "qt/FilesystemPath.h"

#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {
        std::filesystem::path localFilePath(const QUrl& url) {
            return qt::toFileSystemPath(url.toLocalFile());
        }

        std::string savePresetFile(const std::shared_ptr<ports::IFilterPresetStore>& store,
                                   const std::filesystem::path& path,
                                   const ports::FilterPresetSnapshot& snapshot) {
            try {
                store->save(path, snapshot);
                return {};
            } catch (const std::exception& exc) {
                return exc.what();
            } catch (...) {
                return "Erro desconhecido ao exportar filtros";
            }
        }

        FilterPresetLoadResult
        loadPresetFile(const std::shared_ptr<ports::IFilterPresetStore>& store,
                       const std::filesystem::path& path) {
            try {
                return {store->load(path), {}};
            } catch (const std::exception& exc) {
                return {{}, exc.what()};
            } catch (...) {
                return {{}, "Erro desconhecido ao importar filtros"};
            }
        }

        bool hasFilterState(const ports::FilterPreferencesSnapshot& filters) {
            return !filters.searchText.empty() || !filters.quickSector.empty() ||
                   !filters.columnFilters.empty() || !filters.advancedTextFilters.empty() ||
                   !filters.advancedYear.empty() || !filters.advancedWeek.empty() ||
                   !filters.issueYear.empty() || !filters.executionYear.empty() ||
                   !filters.reprogrammingValues.empty() || !filters.issueWeekStart.empty() ||
                   !filters.issueWeekEnd.empty() || !filters.executionWeekStart.empty() ||
                   !filters.executionWeekEnd.empty() || filters.derivationMode != "all" ||
                   filters.excludeScaSesSte || filters.onlyReprogrammed;
        }

        bool sameFilters(const ports::FilterPreferencesSnapshot& lhs,
                         const ports::FilterPreferencesSnapshot& rhs) {
            return lhs.searchText == rhs.searchText && lhs.quickSector == rhs.quickSector &&
                   lhs.columnFilters == rhs.columnFilters &&
                   lhs.advancedTextFilters == rhs.advancedTextFilters &&
                   lhs.advancedWeekColumnKey == rhs.advancedWeekColumnKey &&
                   lhs.advancedYear == rhs.advancedYear && lhs.advancedWeek == rhs.advancedWeek &&
                   lhs.issueYear == rhs.issueYear && lhs.executionYear == rhs.executionYear &&
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

        bool savedFilterNameEquals(const ports::SavedFilterSnapshot& filter, const QString& name) {
            return QString::fromStdString(filter.name).compare(name, Qt::CaseInsensitive) == 0;
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

    } // namespace

    MainPreferenceFlowCoordinator::MainPreferenceFlowCoordinator(
        BrowseViewModel& browse, UiSettingsViewModel& ui, ColumnSettingsModel& columns,
        UserPreferencesCoordinator& preferences,
        std::shared_ptr<ports::IFilterPresetStore> presetStore,
        application::FilterPresetService& presetService, QObject* parent)
        : QObject(parent), browse_(browse), ui_(ui), columns_(columns), preferences_(preferences),
          presetStore_(std::move(presetStore)), presetService_(presetService) {
        connect(&exportPresetWatcher_, &QFutureWatcher<void>::finished, this,
                &MainPreferenceFlowCoordinator::finishExportFilterPreset);
        connect(&importPresetWatcher_, &QFutureWatcher<void>::finished, this,
                &MainPreferenceFlowCoordinator::finishImportFilterPreset);
    }

    MainPreferenceFlowCoordinator::~MainPreferenceFlowCoordinator() {
        shutdown();
    }

    void MainPreferenceFlowCoordinator::shutdown() {
        if (shuttingDown_) {
            return;
        }
        shuttingDown_ = true;
        emit shutdownStarted();
        disconnect(&exportPresetWatcher_, nullptr, this, nullptr);
        disconnect(&importPresetWatcher_, nullptr, this, nullptr);
        if (exportPresetWatcher_.isRunning()) {
            exportPresetWatcher_.waitForFinished();
        }
        if (importPresetWatcher_.isRunning()) {
            importPresetWatcher_.waitForFinished();
        }
        exportPresetTask_.reset();
        importPresetTask_.reset();
    }

    ports::UserPreferencesSnapshot MainPreferenceFlowCoordinator::buildPreferencesSnapshot() const {
        ports::UserPreferencesSnapshot snapshot;
        ui_.writePreferences(snapshot);
        browse_.writePreferences(snapshot);
        snapshot.visibleColumns = columns_.visibleKeys();
        snapshot.columnWidths = columns_.columnWidths();
        snapshot.savedFilters = savedFilters_;
        normalizeFilterPreferences(snapshot.filters);
        normalizeSavedFilterPreferences(snapshot.savedFilters);
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
        normalizeSavedFilterPreferences(storedSavedFilters);
        setSavedFilters(std::move(storedSavedFilters));
    }

    void MainPreferenceFlowCoordinator::scheduleSavePreferences() {
        if (shuttingDown_) {
            return;
        }
        preferences_.scheduleSave([this] { return buildPreferencesSnapshot(); });
    }

    void MainPreferenceFlowCoordinator::saveAppliedColumnPreferences(
        std::vector<std::string> visibleColumns, std::map<std::string, int> columnWidths) {
        if (shuttingDown_) {
            return;
        }
        auto snapshot = buildPreferencesSnapshot();
        snapshot.visibleColumns = std::move(visibleColumns);
        snapshot.columnWidths = std::move(columnWidths);
        preferences_.saveNowOrSchedule(std::move(snapshot));
    }

    void MainPreferenceFlowCoordinator::saveNowOrSchedule() {
        if (shuttingDown_) {
            return;
        }
        preferences_.saveNowOrSchedule(buildPreferencesSnapshot());
    }

    void MainPreferenceFlowCoordinator::savePreferences() {
        if (shuttingDown_) {
            return;
        }
        saveNowOrSchedule();
        emit this->statusMessageRequested("Salvamento solicitado");
    }

    bool MainPreferenceFlowCoordinator::hasActiveFilter() const {
        const auto snapshot = buildPreferencesSnapshot();
        auto filters = snapshot.filters;
        normalizeFilterPreferences(filters);
        return hasFilterState(filters);
    }

    void MainPreferenceFlowCoordinator::notifyNoActiveFilter() {
        if (shuttingDown_) {
            return;
        }
        emit this->statusMessageRequested("Aplique algum filtro antes de salvar");
    }

    QString MainPreferenceFlowCoordinator::suggestedFilterName() const {
        const auto snapshot = buildPreferencesSnapshot();
        auto searchText = QString::fromStdString(snapshot.filters.searchText).trimmed();
        if (!searchText.isEmpty()) {
            return searchText;
        }
        if (!snapshot.filters.quickSector.empty()) {
            return QString::fromStdString(snapshot.filters.quickSector);
        }
        return QStringLiteral("Filtro combinado %1").arg(savedFilters_.size() + 1);
    }

    void MainPreferenceFlowCoordinator::saveCurrentFilter(const QString& name) {
        if (shuttingDown_) {
            return;
        }
        const auto filterName = normalizedName(name);
        if (filterName.isEmpty()) {
            emit this->statusErrorRequested("Informe um nome para salvar o filtro");
            return;
        }
        if (filterName.size() > static_cast<qsizetype>(ports::kMaxSavedFilterNameLength)) {
            emit this->statusErrorRequested("Nome do filtro excede 128 caracteres");
            return;
        }
        if (savedFilters_.size() >= ports::kMaxSavedFilterCount) {
            emit this->statusErrorRequested("Limite de 200 filtros salvos atingido");
            return;
        }
        auto snapshot = buildPreferencesSnapshot();
        normalizeFilterPreferences(snapshot.filters);
        if (!hasFilterState(snapshot.filters)) {
            emit this->statusMessageRequested("Aplique algum filtro antes de salvar");
            return;
        }
        const auto nameExists = std::ranges::any_of(savedFilters_, [&filterName](const auto& item) {
            return savedFilterNameEquals(item, filterName);
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
        if (shuttingDown_) {
            return;
        }
        const auto filterName = normalizedName(name);
        const auto filter = std::ranges::find_if(savedFilters_, [&filterName](const auto& item) {
            return savedFilterNameEquals(item, filterName);
        });
        if (filter == savedFilters_.end()) {
            emit this->statusErrorRequested("Filtro salvo nao encontrado");
            return;
        }
        auto snapshot = buildPreferencesSnapshot();
        snapshot.filters = filter->filters;
        normalizeFilterPreferences(snapshot.filters);
        snapshot.savedFilters = savedFilters_;
        applyStoredPreferences(snapshot);
        browse_.apply();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtro aplicado");
    }

    void MainPreferenceFlowCoordinator::removeSavedFilter(const QString& name) {
        if (shuttingDown_) {
            return;
        }
        const auto filterName = normalizedName(name);
        const auto previousSize = savedFilters_.size();
        std::erase_if(savedFilters_, [&filterName](const auto& item) {
            return savedFilterNameEquals(item, filterName);
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
        if (shuttingDown_) {
            return;
        }
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
        const auto task = std::make_shared<ExportPresetTaskState>();
        exportPresetTask_ = task;
        exportPresetWatcher_.setFuture(QtConcurrent::run(
            [store, path = localFilePath(outputUrl),
             snapshot = presetService_.createPresetWithClearedSearch(buildPreferencesSnapshot()),
             task] {
                auto error = savePresetFile(store, path, snapshot);
                const std::scoped_lock lock(task->mutex);
                task->error = std::move(error);
            }));
        emit this->statusMessageRequested("Exportando filtros");
    }

    void MainPreferenceFlowCoordinator::importFilterPreset(const QUrl& inputUrl) {
        if (shuttingDown_) {
            return;
        }
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
        const auto task = std::make_shared<ImportPresetTaskState>();
        importPresetTask_ = task;
        importPresetWatcher_.setFuture(
            QtConcurrent::run([store, path = localFilePath(inputUrl), task] {
                auto result = loadPresetFile(store, path);
                const std::scoped_lock lock(task->mutex);
                task->snapshot = std::move(result.snapshot);
                task->error = std::move(result.error);
            }));
        emit this->statusMessageRequested("Importando filtros");
    }

    void MainPreferenceFlowCoordinator::finishExportFilterPreset() {
        if (shuttingDown_) {
            return;
        }
        std::string error;
        const auto task = exportPresetTask_;
        if (task) {
            const std::scoped_lock lock(task->mutex);
            error = task->error;
        }
        exportPresetTask_.reset();
        if (error.empty()) {
            emit this->statusErrorClearRequested();
            emit this->statusMessageRequested("Filtros exportados");
        } else {
            emit this->statusErrorRequested(QString::fromStdString(error));
        }
    }

    void MainPreferenceFlowCoordinator::finishImportFilterPreset() {
        if (shuttingDown_) {
            return;
        }
        FilterPresetLoadResult result;
        const auto task = importPresetTask_;
        if (task) {
            const std::scoped_lock lock(task->mutex);
            if (task->snapshot) {
                result.snapshot = *task->snapshot;
            }
            result.error = task->error;
        }
        importPresetTask_.reset();
        if (!result.error.empty()) {
            emit this->statusErrorRequested(QString::fromStdString(result.error));
            return;
        }
        auto baseSnapshot = buildPreferencesSnapshot();
        presetService_.applyPresetPreservingSearch(result.snapshot, baseSnapshot);
        normalizeFilterPreferences(baseSnapshot.filters);
        applyStoredPreferences(baseSnapshot);
        browse_.apply();
        emit this->statusErrorClearRequested();
        emit this->statusMessageRequested("Filtros importados");
    }

    void MainPreferenceFlowCoordinator::requestSaveFromSignal() {
        if (shuttingDown_) {
            return;
        }
        scheduleSavePreferences();
    }

    void MainPreferenceFlowCoordinator::setSavedFilters(
        std::vector<ports::SavedFilterSnapshot> filters) {
        sortSavedFilters(filters);
        normalizeSavedFilterPreferences(filters);
        if (sameSavedFilters(savedFilters_, filters)) {
            return;
        }
        savedFilters_ = std::move(filters);
        emit savedFiltersChanged();
    }

} // namespace ssa::presentation
