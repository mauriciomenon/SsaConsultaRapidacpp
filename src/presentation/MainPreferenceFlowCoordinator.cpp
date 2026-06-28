#include "presentation/MainPreferenceFlowCoordinator.h"

#include "presentation/UserPreferencesCoordinator.h"

#include <QCoreApplication>
#include <QtConcurrent>

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

    void MainPreferenceFlowCoordinator::applyStoredPreferences(
        const ports::UserPreferencesSnapshot& snapshot) {
        ui_.applyPreferences(snapshot);
        browse_.applyPreferences(snapshot);
        columns_.applyPreferences(snapshot);
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

} // namespace ssa::presentation
