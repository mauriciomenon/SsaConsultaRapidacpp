#include "presentation/MainViewModel.h"

#include "domain/SsaTypes.h"

#include <QClipboard>
#include <QGuiApplication>

#include <utility>

namespace ssa::presentation {

    MainViewModel::MainViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                 std::shared_ptr<ports::IExternalCommandPort> commandPort,
                                 std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
                                 std::shared_ptr<ports::IFilterPresetStore> filterPresetStore,
                                 std::shared_ptr<application::SsaWorkflowService> workflowService,
                                 QObject* parent)
        : QObject(parent), browse_(std::move(queryService), this), columns_(this), ui_(this),
          preferences_(std::move(preferencesStore), this),
          preferencesFlow_(browse_, ui_, columns_, preferences_, std::move(filterPresetStore),
                           filterPresetService_),
          actions_(
              std::move(commandPort), std::move(workflowService),
              [this] { return browse_.currentRequest(); }, *browse_.status(), preferences_, this),
          columnsFlow_(
              browse_, columns_,
              [preferencesFlow = &preferencesFlow_] { preferencesFlow->scheduleSavePreferences(); },
              [preferencesFlow = &preferencesFlow_](std::vector<std::string> visibleColumns,
                                                    std::map<std::string, int> columnWidths) {
                  preferencesFlow->saveAppliedColumnPreferences(std::move(visibleColumns),
                                                                std::move(columnWidths));
              }),
          selectionFlow_(browse_, *actions_.commands()), requestFlow_(browse_) {
        connectPreferenceFlows();
        preferencesFlow_.applyStoredPreferences(preferences_.loadInitial());
        connectWorkflowRefresh();
    }

    MainViewModel::~MainViewModel() {
        auto finalSnapshot = preferencesFlow_.buildPreferencesSnapshot();
        preferencesFlow_.shutdown();
        preferences_.shutdown(std::move(finalSnapshot));
    }

    void MainViewModel::connectPreferenceFlows() {
        connect(&browse_, &BrowseViewModel::preferencesSaveRequested, &preferencesFlow_,
                &MainPreferenceFlowCoordinator::requestSaveFromSignal);
        connect(&ui_, &UiSettingsViewModel::preferencesSaveRequested, &preferencesFlow_,
                &MainPreferenceFlowCoordinator::requestSaveFromSignal);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusMessageRequested,
                browse_.status(), &StatusViewModel::setMessage);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusErrorRequested,
                browse_.status(), &StatusViewModel::setError);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusErrorClearRequested,
                browse_.status(), [status = browse_.status()] { status->setError({}); });
    }

    void MainViewModel::connectWorkflowRefresh() {
        connect(actions_.workflows(), &WorkflowCommandViewModel::lastResultChanged, &browse_,
                [this] {
                    if (actions_.workflows()->lastSucceeded()) {
                        pendingWorkflowRefreshMessage_ = actions_.workflows()->successMessage();
                        browse_.invalidateTotalRowsAll();
                        browse_.apply();
                    }
                });
        connect(&browse_, &BrowseViewModel::pageChanged, this, [this] {
            if (pendingWorkflowRefreshMessage_.isEmpty()) {
                return;
            }
            browse_.status()->setMessage(pendingWorkflowRefreshMessage_);
            pendingWorkflowRefreshMessage_.clear();
        });
    }

    BrowseViewModel* MainViewModel::browse() {
        return &browse_;
    }

    DesktopActionsViewModel* MainViewModel::actions() {
        return &actions_;
    }

    ColumnSettingsModel* MainViewModel::columns() {
        return &columns_;
    }

    UiSettingsViewModel* MainViewModel::ui() {
        return &ui_;
    }

    QObject* MainViewModel::columnFlow() {
        return &columnsFlow_;
    }

    QObject* MainViewModel::selectionFlow() {
        return &selectionFlow_;
    }

    QObject* MainViewModel::requestFlow() {
        return &requestFlow_;
    }

    QObject* MainViewModel::preferenceFlow() {
        return &preferencesFlow_;
    }

    bool MainViewModel::copyTextToClipboard(const QString& text) {
        if (text.isEmpty()) {
            return false;
        }
        auto* clipboard = QGuiApplication::clipboard();
        if (clipboard == nullptr) {
            return false;
        }
        clipboard->setText(text);
        browse_.status()->setMessage(QStringLiteral("Texto copiado"));
        return true;
    }

} // namespace ssa::presentation
