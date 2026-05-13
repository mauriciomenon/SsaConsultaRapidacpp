#include "presentation/MainViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <QPointer>

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {

        std::shared_ptr<ports::IExternalCommandPort>
        requireCommandPort(std::shared_ptr<ports::IExternalCommandPort> commandPort) {
            if (!commandPort) {
                throw std::invalid_argument("external command port is required");
            }
            return commandPort;
        }

    } // namespace

    MainViewModel::MainViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                 std::shared_ptr<ports::IExternalCommandPort> commandPort,
                                 std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
                                 std::shared_ptr<application::SsaWorkflowService> workflowService,
                                 QObject* parent)
        : QObject(parent), browse_(std::move(queryService), this),
          commands_(requireCommandPort(std::move(commandPort)), nullptr, this),
          exports_(
              std::move(workflowService),
              [this] {
                  auto request = browse_.currentRequest();
                  request.pageIndex = 0;
                  return request;
              },
              nullptr, this),
          columns_(this), ui_(this), preferences_(std::move(preferencesStore), this) {
        connect(&browse_, &BrowseViewModel::pageChanged, this, &MainViewModel::pageChanged);
        connect(&browse_, &BrowseViewModel::sortChanged, this, &MainViewModel::sortChanged);
        connect(&browse_, &BrowseViewModel::preferencesSaveRequested, this,
                &MainViewModel::scheduleSavePreferences);
        connect(&ui_, &UiSettingsViewModel::preferencesSaveRequested, this,
                &MainViewModel::scheduleSavePreferences);
        connect(&ui_, &UiSettingsViewModel::settingsChanged, this,
                &MainViewModel::preferencesChanged);
        connect(&preferences_, &UserPreferencesCoordinator::saveFailed, this,
                [this](const QString& message) {
                    browse_.status()->setError(message);
                    browse_.status()->setMessage("Falha ao salvar preferencias");
                });
        connect(&exports_, &ExportViewModel::runningChanged, this, [this] {
            if (exports_.running()) {
                browse_.status()->setError({});
                browse_.status()->setMessage("Exportando dados...");
            }
        });
        connect(&exports_, &ExportViewModel::lastResultChanged, this, [this] {
            if (exports_.lastSucceeded()) {
                browse_.status()->setError({});
                browse_.status()->setMessage("Exportacao concluida");
            } else {
                browse_.status()->setError(exports_.lastMessage());
                browse_.status()->setMessage("Falha ao exportar dados");
            }
        });
        connect(&commands_, &CommandViewModel::runningChanged, this, [this] {
            if (commands_.running()) {
                browse_.status()->setError({});
                browse_.status()->setMessage("Executando comando...");
            }
        });
        connect(&commands_, &CommandViewModel::lastResultChanged, this, [this] {
            if (commands_.lastSucceeded()) {
                browse_.status()->setError({});
                browse_.status()->setMessage("Comando concluido");
            } else {
                browse_.status()->setError(commands_.lastMessage());
                browse_.status()->setMessage("Falha ao executar comando");
            }
        });
        applyPreferences(preferences_.loadInitial());
    }

    BrowseViewModel* MainViewModel::browse() {
        return &browse_;
    }

    CommandViewModel* MainViewModel::commands() {
        return &commands_;
    }

    ExportViewModel* MainViewModel::exports() {
        return &exports_;
    }

    ColumnSettingsModel* MainViewModel::columns() {
        return &columns_;
    }

    UiSettingsViewModel* MainViewModel::ui() {
        return &ui_;
    }

    void MainViewModel::applyColumnSettings() {
        browse_.applyColumnSettings(columns_.visibleKeys(), columns_.columnWidths());
        scheduleSavePreferences();
    }

    void MainViewModel::resetColumnSettings() {
        columns_.resetDefaults();
    }

    void MainViewModel::discardColumnSettings() {
        columns_.applyPreferences(browse_.visibleColumns(), browse_.columnWidths());
    }

    void MainViewModel::openSelectedSsa() {
        const auto selected = browse_.details()->selectedSsa();
        if (!selected.isEmpty()) {
            commands_.openSsa(selected);
        }
    }

    void MainViewModel::cancelCurrentRequest() {
        browse_.cancelCurrentRequest();
    }

    ports::UserPreferencesSnapshot MainViewModel::buildPreferencesSnapshot() const {
        ports::UserPreferencesSnapshot snapshot;
        ui_.writePreferences(snapshot);
        browse_.writePreferences(snapshot);
        return snapshot;
    }

    void MainViewModel::applyPreferences(ports::UserPreferencesSnapshot snapshot) {
        ui_.applyPreferences(snapshot);
        browse_.applyPreferences(snapshot);
        ports::UserPreferencesSnapshot appliedBrowseSnapshot;
        browse_.writePreferences(appliedBrowseSnapshot);
        columns_.applyPreferences(appliedBrowseSnapshot.visibleColumns,
                                  appliedBrowseSnapshot.columnWidths);
        emit preferencesChanged();
    }

    void MainViewModel::scheduleSavePreferences() {
        const QPointer<MainViewModel> self{this};
        preferences_.scheduleSave([self] {
            return self ? self->buildPreferencesSnapshot() : ports::UserPreferencesSnapshot{};
        });
    }

    void MainViewModel::savePreferences() {
        preferences_.saveNowOrSchedule(buildPreferencesSnapshot());
    }

} // namespace ssa::presentation
