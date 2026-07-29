#pragma once

#include "application/FilterPresetService.h"
#include "domain/SsaTypes.h"
#include "ports/IExecutadasReportPort.h"
#include "ports/ISsaBrowsePort.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/ActivityAnalyticsViewModel.h"
#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"
#include "presentation/DataSetupViewModel.h"
#include "presentation/DatabaseSwitchViewModel.h"
#include "presentation/DesktopActionsViewModel.h"
#include "presentation/MainColumnFlowCoordinator.h"
#include "presentation/MainPreferenceFlowCoordinator.h"
#include "presentation/MainRequestFlowCoordinator.h"
#include "presentation/MainSelectionFlowCoordinator.h"
#include "presentation/RecentLogModel.h"
#include "presentation/UiSettingsViewModel.h"
#include "presentation/UserPreferencesCoordinator.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace ssa::presentation {

    class MainViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(BrowseViewModel* browse READ browse CONSTANT)
        Q_PROPERTY(DesktopActionsViewModel* actions READ actions CONSTANT)
        Q_PROPERTY(ColumnSettingsModel* columns READ columns CONSTANT)
        Q_PROPERTY(UiSettingsViewModel* ui READ ui CONSTANT)
        Q_PROPERTY(DatabaseSwitchViewModel* databaseSwitch READ databaseSwitch CONSTANT)
        Q_PROPERTY(DataSetupViewModel* dataSetup READ dataSetup CONSTANT)
        Q_PROPERTY(bool hasDatabase READ hasDatabase CONSTANT)
        Q_PROPERTY(ActivityAnalyticsViewModel* analytics READ analytics CONSTANT)
        Q_PROPERTY(RecentLogModel* logs READ logs CONSTANT)
        Q_PROPERTY(QObject* columnFlow READ columnFlow CONSTANT)
        Q_PROPERTY(QObject* selectionFlow READ selectionFlow CONSTANT)
        Q_PROPERTY(QObject* requestFlow READ requestFlow CONSTANT)
        Q_PROPERTY(QObject* preferenceFlow READ preferenceFlow CONSTANT)
        Q_PROPERTY(bool shutdownInProgress READ shutdownInProgress NOTIFY shutdownStateChanged)
        Q_PROPERTY(bool shutdownReady READ shutdownReady NOTIFY shutdownStateChanged)
        Q_PROPERTY(bool forceCloseAvailable READ forceCloseAvailable NOTIFY shutdownStateChanged)
        Q_PROPERTY(bool canCancelActivity READ canCancelActivity NOTIFY activityStateChanged)
        Q_PROPERTY(bool cancelingActivity READ cancelingActivity NOTIFY activityStateChanged)

      public:
        MainViewModel(
            std::shared_ptr<ports::ISsaBrowsePort> browsePort,
            std::shared_ptr<ports::IExternalCommandPort> commandPort,
            std::shared_ptr<ports::IUserPreferencesStore> preferencesStore = nullptr,
            std::shared_ptr<ports::IFilterPresetStore> filterPresetStore = nullptr,
            std::shared_ptr<application::SsaWorkflowService> workflowService = nullptr,
            std::shared_ptr<ports::IDatabaseValidator> databaseValidator = nullptr,
            std::shared_ptr<ports::IApplicationLauncher> applicationLauncher = nullptr,
            std::shared_ptr<ports::IExecutadasReportPort> reportPort = nullptr,
            QObject* parent = nullptr,
            std::shared_ptr<const application::ActivityAnalyticsService> analyticsService = nullptr,
            std::shared_ptr<ports::IDataSetupPort> dataSetupPort = nullptr,
            QString defaultHomeRoot = {}, bool hasDatabase = true);
        ~MainViewModel() override;

        [[nodiscard]] BrowseViewModel* browse();
        [[nodiscard]] DesktopActionsViewModel* actions();
        [[nodiscard]] ColumnSettingsModel* columns();
        [[nodiscard]] UiSettingsViewModel* ui();
        [[nodiscard]] DatabaseSwitchViewModel* databaseSwitch();
        [[nodiscard]] DataSetupViewModel* dataSetup();
        [[nodiscard]] bool hasDatabase() const;
        [[nodiscard]] ActivityAnalyticsViewModel* analytics();
        [[nodiscard]] RecentLogModel* logs();
        [[nodiscard]] QObject* columnFlow();
        [[nodiscard]] QObject* selectionFlow();
        [[nodiscard]] QObject* requestFlow();
        [[nodiscard]] QObject* preferenceFlow();
        Q_INVOKABLE bool copyTextToClipboard(const QString& text);
        [[nodiscard]] bool shutdownInProgress() const;
        [[nodiscard]] bool shutdownReady() const;
        [[nodiscard]] bool forceCloseAvailable() const;
        [[nodiscard]] bool canCancelActivity();
        [[nodiscard]] bool cancelingActivity();
        Q_INVOKABLE void requestCancelAll();
        Q_INVOKABLE void requestShutdown();
        Q_INVOKABLE void requestForcedShutdown();

      signals:
        void shutdownStateChanged();
        void activityStateChanged();
        void forcedShutdownRequested();

      private:
        void connectPreferenceFlows();
        void connectWorkflowRefresh();
        void handleActivityStateChanged();
        void checkShutdownReady();
        [[nodiscard]] bool hasActiveOperations();

        BrowseViewModel browse_;
        ColumnSettingsModel columns_;
        UiSettingsViewModel ui_;
        UserPreferencesCoordinator preferences_;
        application::FilterPresetService filterPresetService_;
        DatabaseSwitchViewModel databaseSwitch_;
        DataSetupViewModel dataSetup_;
        RecentLogModel logs_;
        DesktopActionsViewModel actions_;
        MainPreferenceFlowCoordinator preferencesFlow_;
        MainColumnFlowCoordinator columnsFlow_;
        MainSelectionFlowCoordinator selectionFlow_;
        MainRequestFlowCoordinator requestFlow_;
        QString pendingWorkflowRefreshMessage_;
        QString pendingWorkflowRefreshWarning_;
        QTimer shutdownPoll_;
        QTimer forceCloseTimer_;
        bool shutdownInProgress_{false};
        bool shutdownReady_{false};
        bool forceCloseAvailable_{false};
        bool backgroundCanceling_{false};
        bool hasDatabase_{false};
        std::unique_ptr<ActivityAnalyticsViewModel> analytics_;
    };

} // namespace ssa::presentation
