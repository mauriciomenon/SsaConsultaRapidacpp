#include "presentation/MainViewModel.h"

#include "domain/SsaTypes.h"

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>

#include <utility>

namespace ssa::presentation {

    MainViewModel::MainViewModel(
        std::shared_ptr<ports::ISsaBrowsePort> browsePort,
        std::shared_ptr<ports::IExternalCommandPort> commandPort,
        std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
        std::shared_ptr<ports::IFilterPresetStore> filterPresetStore,
        std::shared_ptr<application::SsaWorkflowService> workflowService,
        std::shared_ptr<ports::IDatabaseValidator> databaseValidator,
        std::shared_ptr<ports::IApplicationLauncher> applicationLauncher,
        std::shared_ptr<ports::IExecutadasReportPort> reportPort, QObject* parent,
        std::shared_ptr<const application::ActivityAnalyticsService> analyticsService,
        std::shared_ptr<ports::IDataSetupPort> dataSetupPort, QString defaultHomeRoot,
        const bool hasDatabase)
        : QObject(parent), browse_(std::move(browsePort), std::move(reportPort), this),
          columns_(this), ui_(this), preferences_(std::move(preferencesStore), this),
          databaseSwitch_(std::move(databaseValidator), applicationLauncher, this),
          dataSetup_(std::move(dataSetupPort), std::move(applicationLauncher),
                     std::move(defaultHomeRoot), this),
          logs_(this),
          actions_(
              std::move(commandPort), std::move(workflowService),
              [this] { return browse_.currentRequest(); }, *browse_.status(), preferences_, this),
          preferencesFlow_(browse_, ui_, columns_, *actions_.workflows(), preferences_,
                           std::move(filterPresetStore), filterPresetService_),
          columnsFlow_(
              browse_, columns_,
              [preferencesFlow = &preferencesFlow_] { preferencesFlow->scheduleSavePreferences(); },
              [preferencesFlow = &preferencesFlow_](std::vector<std::string> visibleColumns,
                                                    std::map<std::string, int> columnWidths) {
                  preferencesFlow->saveAppliedColumnPreferences(std::move(visibleColumns),
                                                                std::move(columnWidths));
              }),
          selectionFlow_(browse_, *actions_.commands()), requestFlow_(browse_),
          hasDatabase_(hasDatabase) {
        if (analyticsService) {
            analytics_ =
                std::make_unique<ActivityAnalyticsViewModel>(std::move(analyticsService), this);
            connect(analytics_.get(), &ActivityAnalyticsViewModel::activeOperationsChanged, this,
                    &MainViewModel::handleActivityStateChanged);
            connect(analytics_.get(), &ActivityAnalyticsViewModel::stateChanged, this,
                    &MainViewModel::handleActivityStateChanged);
        }
        shutdownPoll_.setInterval(25);
        connect(&shutdownPoll_, &QTimer::timeout, this, &MainViewModel::checkShutdownReady);
        forceCloseTimer_.setSingleShot(true);
        forceCloseTimer_.setInterval(10'000);
        connect(&forceCloseTimer_, &QTimer::timeout, this, [this] {
            forceCloseAvailable_ = true;
            emit shutdownStateChanged();
        });
        connect(browse_.status(), &StatusViewModel::changed, this,
                &MainViewModel::handleActivityStateChanged);
        connect(browse_.status(), &StatusViewModel::changed, this, [this] {
            const auto error = browse_.status()->error();
            if (!error.isEmpty() && error != actions_.workflows()->lastMessage()) {
                logs_.append(QStringLiteral("Error"), QStringLiteral("Status"), error);
            }
        });
        connect(actions_.workflows(), &WorkflowCommandViewModel::logEntryRequested, &logs_,
                &RecentLogModel::append);
        connect(actions_.workflows(), &WorkflowCommandViewModel::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(actions_.exports(), &ExportViewModel::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(&databaseSwitch_, &DatabaseSwitchViewModel::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(&dataSetup_, &DataSetupViewModel::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(&preferences_, &UserPreferencesCoordinator::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::stateChanged, this,
                &MainViewModel::handleActivityStateChanged);
        connect(&browse_, &BrowseViewModel::backgroundActivityChanged, this, [this] {
            if (backgroundCanceling_ && !browse_.backgroundWorkRunning()) {
                backgroundCanceling_ = false;
            }
            handleActivityStateChanged();
        });
        connectPreferenceFlows();
        preferencesFlow_.applyStoredPreferences(preferences_.loadInitial());
        connectWorkflowRefresh();
    }

    MainViewModel::~MainViewModel() {
        shutdownPoll_.stop();
        forceCloseTimer_.stop();
        requestCancelAll();
        preferencesFlow_.shutdown();
        preferences_.shutdown();
        databaseSwitch_.shutdown();
        dataSetup_.shutdown();
    }

    void MainViewModel::connectPreferenceFlows() {
        connect(&browse_, &BrowseViewModel::preferencesSaveRequested, &preferencesFlow_,
                &MainPreferenceFlowCoordinator::requestSaveFromSignal);
        connect(&ui_, &UiSettingsViewModel::preferencesSaveRequested, &preferencesFlow_,
                &MainPreferenceFlowCoordinator::requestSaveFromSignal);
        connect(actions_.workflows(), &WorkflowCommandViewModel::preferencesSaveRequested,
                &preferencesFlow_, &MainPreferenceFlowCoordinator::requestSaveFromSignal);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusMessageRequested,
                browse_.status(), &StatusViewModel::setMessage);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusErrorRequested,
                browse_.status(), &StatusViewModel::setError);
        connect(&preferencesFlow_, &MainPreferenceFlowCoordinator::statusErrorClearRequested,
                browse_.status(), [status = browse_.status()] { status->setError({}); });
    }

    void MainViewModel::connectWorkflowRefresh() {
        connect(
            actions_.workflows(), &WorkflowCommandViewModel::lastResultChanged, &browse_, [this] {
                if (actions_.workflows()->lastSucceeded()) {
                    if (analytics_) {
                        analytics_->invalidateAfterImport();
                    }
                    pendingWorkflowRefreshMessage_ = actions_.workflows()->lastWarning()
                                                         ? actions_.workflows()->lastMessage()
                                                         : actions_.workflows()->successMessage();
                    browse_.invalidateTotalRowsAll();
                    browse_.filters()->invalidateDataSourceOptions();
                    browse_.apply();
                }
            });
        connect(&browse_, &BrowseViewModel::pageChanged, this, [this] {
            if (pendingWorkflowRefreshMessage_.isEmpty()) {
                return;
            }
            browse_.status()->setMessage(pendingWorkflowRefreshMessage_);
            browse_.status()->setError({});
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

    DatabaseSwitchViewModel* MainViewModel::databaseSwitch() {
        return &databaseSwitch_;
    }

    DataSetupViewModel* MainViewModel::dataSetup() {
        return &dataSetup_;
    }

    bool MainViewModel::hasDatabase() const {
        return hasDatabase_;
    }

    ActivityAnalyticsViewModel* MainViewModel::analytics() {
        return analytics_.get();
    }

    RecentLogModel* MainViewModel::logs() {
        return &logs_;
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

    bool MainViewModel::shutdownInProgress() const {
        return shutdownInProgress_;
    }

    bool MainViewModel::shutdownReady() const {
        return shutdownReady_;
    }

    bool MainViewModel::forceCloseAvailable() const {
        return forceCloseAvailable_;
    }

    bool MainViewModel::canCancelActivity() {
        if (shutdownInProgress_) {
            return false;
        }
        const bool queryCancelable = browse_.status()->loading() &&
                                     browse_.status()->message() != QStringLiteral("Cancelando...");
        return queryCancelable || (browse_.backgroundWorkRunning() && !backgroundCanceling_) ||
               actions_.workflows()->canCancel() || actions_.exports()->canCancel() ||
               databaseSwitch_.canCancel() || dataSetup_.canCancel() || preferences_.canCancel() ||
               preferencesFlow_.canCancel() || (analytics_ && analytics_->canCancel());
    }

    bool MainViewModel::cancelingActivity() {
        return backgroundCanceling_ || actions_.workflows()->canceling() ||
               actions_.exports()->canceling() || databaseSwitch_.canceling() ||
               dataSetup_.canceling() || preferences_.canceling() || preferencesFlow_.canceling() ||
               (analytics_ && analytics_->canceling()) ||
               (browse_.status()->loading() &&
                browse_.status()->message() == QStringLiteral("Cancelando..."));
    }

    void MainViewModel::requestCancelAll() {
        const bool hadActivity = hasActiveOperations();
        backgroundCanceling_ = backgroundCanceling_ || browse_.backgroundWorkRunning();
        browse_.cancelBackgroundWork();
        actions_.workflows()->cancel();
        actions_.exports()->cancel();
        databaseSwitch_.cancel();
        dataSetup_.cancel();
        if (analytics_) {
            analytics_->cancel();
        }
        preferencesFlow_.cancel();
        preferences_.cancel();
        if (hadActivity) {
            browse_.status()->setMessage(QStringLiteral("Cancelando..."));
        }
        emit activityStateChanged();
    }

    void MainViewModel::requestShutdown() {
        if (shutdownInProgress_) {
            return;
        }
        shutdownInProgress_ = true;
        emit shutdownStateChanged();
        std::optional<ports::UserPreferencesSnapshot> finalSnapshot;
        try {
            finalSnapshot.emplace(preferencesFlow_.buildPreferencesSnapshot());
        } catch (const std::exception& exception) {
            qWarning() << "Failed to build final preferences snapshot:" << exception.what();
        } catch (...) {
            qWarning() << "Failed to build final preferences snapshot: unknown error";
        }
        requestCancelAll();
        preferencesFlow_.shutdown();
        preferences_.beginShutdown(std::move(finalSnapshot));
        checkShutdownReady();
        if (!shutdownReady_) {
            shutdownPoll_.start();
            forceCloseTimer_.start();
        }
    }

    void MainViewModel::requestForcedShutdown() {
        if (!shutdownInProgress_ || !forceCloseAvailable_) {
            return;
        }
        requestCancelAll();
        emit forcedShutdownRequested();
    }

    bool MainViewModel::hasActiveOperations() {
        return browse_.status()->loading() || actions_.workflows()->running() ||
               actions_.exports()->running() || databaseSwitch_.running() || dataSetup_.running() ||
               actions_.commands()->running() || preferences_.running() ||
               preferencesFlow_.running() || browse_.backgroundWorkRunning() ||
               (analytics_ && analytics_->hasActiveOperations());
    }

    void MainViewModel::handleActivityStateChanged() {
        if (!hasActiveOperations() && !cancelingActivity() &&
            browse_.status()->message() == QStringLiteral("Cancelando...")) {
            browse_.status()->setMessage(QStringLiteral("Operacao cancelada"));
            if (!browse_.status()->error().isEmpty()) {
                browse_.status()->setError({});
            }
            return;
        }
        emit activityStateChanged();
    }

    void MainViewModel::checkShutdownReady() {
        if (!shutdownInProgress_ || shutdownReady_ || hasActiveOperations()) {
            return;
        }
        shutdownPoll_.stop();
        forceCloseTimer_.stop();
        shutdownReady_ = true;
        emit shutdownStateChanged();
    }

} // namespace ssa::presentation
