#include "presentation/MainViewModel.h"

#include "domain/SsaTypes.h"

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
          columnsFlow_(browse_, columns_,
                       [preferencesFlow = &preferencesFlow_] {
                           preferencesFlow->scheduleSavePreferences();
                       }),
          selectionFlow_(browse_, *actions_.commands()), requestFlow_(browse_) {
        preferencesFlow_.applyStoredPreferences(preferences_.loadInitial());
        connectPreferenceFlows();
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

} // namespace ssa::presentation
