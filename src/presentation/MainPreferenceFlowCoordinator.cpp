#include "presentation/MainPreferenceFlowCoordinator.h"

#include "presentation/UserPreferencesCoordinator.h"

#include <QtConcurrent>

#include <QtGlobal>

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
        return snapshot;
    }

    void
    MainPreferenceFlowCoordinator::applyStoredPreferences(ports::UserPreferencesSnapshot snapshot) {
        ui_.applyPreferences(snapshot);
        browse_.applyPreferences(snapshot);
        columns_.applyPreferences(snapshot);
    }

    void MainPreferenceFlowCoordinator::scheduleSavePreferences() const {
        preferences_.scheduleSave([this] { return buildPreferencesSnapshot(); });
    }

    void MainPreferenceFlowCoordinator::saveNowOrSchedule() const {
        preferences_.saveNowOrSchedule(buildPreferencesSnapshot());
    }

    void MainPreferenceFlowCoordinator::savePreferences() {
        saveNowOrSchedule();
        emit statusMessageRequested("Salvamento solicitado");
    }

    void MainPreferenceFlowCoordinator::exportFilterPreset(const QUrl& outputUrl) {
        if (!presetStore_) {
            emit statusErrorRequested("Exportacao de filtros nao configurada");
            return;
        }
        if (!outputUrl.isLocalFile()) {
            emit statusErrorRequested("Preset de filtros deve ser arquivo local");
            return;
        }
        if (exportPresetWatcher_.isRunning() || importPresetWatcher_.isRunning()) {
            emit statusErrorRequested("Operacao de filtros em andamento");
            return;
        }

        const auto store = presetStore_;
        exportPresetWatcher_.setFuture(QtConcurrent::run(
            [store, path = localFilePath(outputUrl),
             snapshot = presetService_.createPresetWithClearedSearch(buildPreferencesSnapshot())] {
                return savePresetFile(store, path, snapshot);
            }));
        emit statusMessageRequested("Exportando filtros");
    }

    void MainPreferenceFlowCoordinator::importFilterPreset(const QUrl& inputUrl) {
        if (!presetStore_) {
            emit statusErrorRequested("Importacao de filtros nao configurada");
            return;
        }
        if (!inputUrl.isLocalFile()) {
            emit statusErrorRequested("Preset de filtros deve ser arquivo local");
            return;
        }
        if (exportPresetWatcher_.isRunning() || importPresetWatcher_.isRunning()) {
            emit statusErrorRequested("Operacao de filtros em andamento");
            return;
        }

        const auto store = presetStore_;
        importPresetWatcher_.setFuture(QtConcurrent::run(
            [store, path = localFilePath(inputUrl)] { return loadPresetFile(store, path); }));
        emit statusMessageRequested("Importando filtros");
    }

    void MainPreferenceFlowCoordinator::finishExportFilterPreset() {
        const auto error = exportPresetWatcher_.future().result();
        if (error.isEmpty()) {
            emit statusErrorClearRequested();
            emit statusMessageRequested("Filtros exportados");
        } else {
            emit statusErrorRequested(error);
        }
    }

    void MainPreferenceFlowCoordinator::finishImportFilterPreset() {
        const auto result = importPresetWatcher_.future().result();
        if (!result.error.isEmpty()) {
            emit statusErrorRequested(result.error);
            return;
        }
        auto baseSnapshot = buildPreferencesSnapshot();
        presetService_.applyPresetPreservingSearch(result.snapshot, baseSnapshot);
        applyStoredPreferences(std::move(baseSnapshot));
        browse_.apply();
        emit statusErrorClearRequested();
        emit statusMessageRequested("Filtros importados");
    }

    void MainPreferenceFlowCoordinator::waitForPresetTasks() {
        if (exportPresetWatcher_.isRunning()) {
            exportPresetWatcher_.waitForFinished();
        }
        if (importPresetWatcher_.isRunning()) {
            importPresetWatcher_.waitForFinished();
        }
    }

    void MainPreferenceFlowCoordinator::requestSaveFromSignal() {
        scheduleSavePreferences();
    }

} // namespace ssa::presentation
