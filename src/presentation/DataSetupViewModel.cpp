#include "presentation/DataSetupViewModel.h"

#include "presentation/AsyncOperationErrorLog.h"
#include "qt/FilesystemPath.h"

#include <QDebug>
#include <QThreadPool>
#include <QtConcurrentRun>

#include <utility>

namespace ssa::presentation {

    DataSetupViewModel::DataSetupViewModel(std::shared_ptr<ports::IDataSetupPort> setupPort,
                                           std::shared_ptr<ports::IApplicationLauncher> launcher,
                                           QString defaultHomeRoot, QObject* parent)
        : QObject(parent), setupPort_(std::move(setupPort)), launcher_(std::move(launcher)),
          defaultHomeRoot_(std::move(defaultHomeRoot)) {
        connect(&watcher_, &QFutureWatcher<void>::finished, this,
                &DataSetupViewModel::finishExecution);
    }

    DataSetupViewModel::~DataSetupViewModel() {
        shutdown();
    }

    bool DataSetupViewModel::running() const {
        return running_;
    }

    bool DataSetupViewModel::canCancel() const {
        return running_ && !canceling_;
    }

    bool DataSetupViewModel::canceling() const {
        return canceling_;
    }

    QString DataSetupViewModel::errorMessage() const {
        return errorMessage_;
    }

    QString DataSetupViewModel::progressMessage() const {
        return progressMessage_;
    }

    QString DataSetupViewModel::destinationPath() const {
        const auto root = selectedRoot();
        return root.empty() ? QString{} : qt::toQString(root / "data" / "ssas.db");
    }

    int DataSetupViewModel::action() const {
        return action_;
    }

    int DataSetupViewModel::destinationMode() const {
        return destinationMode_;
    }

    void DataSetupViewModel::setAction(const int action) {
        if (running_ || pendingLaunchTargets_ || action < 0 || action > 3 || action_ == action) {
            return;
        }
        action_ = action;
        setErrorMessage({});
        emit selectionChanged();
    }

    void DataSetupViewModel::setDestinationMode(const int mode) {
        if (running_ || pendingLaunchTargets_ || mode < 0 || mode > 1 || destinationMode_ == mode) {
            return;
        }
        destinationMode_ = mode;
        setErrorMessage({});
        emit selectionChanged();
    }

    void DataSetupViewModel::setCustomDestination(const QUrl& url) {
        if (running_ || pendingLaunchTargets_) {
            return;
        }
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            customDestination_ = QUrl{};
            setErrorMessage(QStringLiteral("Selecione uma pasta raiz local"));
            emit selectionChanged();
            return;
        }
        customDestination_ = url;
        setErrorMessage({});
        emit selectionChanged();
    }

    void DataSetupViewModel::setSourceDatabase(const QUrl& url) {
        if (running_ || pendingLaunchTargets_) {
            return;
        }
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            sourceDatabase_ = QUrl{};
            setErrorMessage(QStringLiteral("Selecione um banco de dados local"));
            return;
        }
        sourceDatabase_ = url;
        setErrorMessage({});
    }

    void DataSetupViewModel::setXlsxFiles(const QVariantList& files) {
        if (running_ || pendingLaunchTargets_) {
            return;
        }
        std::vector<std::filesystem::path> selected;
        selected.reserve(static_cast<std::size_t>(files.size()));
        for (const auto& value : files) {
            const auto url = value.toUrl();
            if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
                xlsxFiles_.clear();
                setErrorMessage(QStringLiteral("Selecione apenas planilhas XLSX locais"));
                return;
            }
            selected.push_back(qt::toFileSystemPath(url.toLocalFile()));
        }
        xlsxFiles_ = std::move(selected);
        setErrorMessage({});
    }

    void DataSetupViewModel::execute() {
        if (running_ || shuttingDown_) {
            return;
        }
        if (!launcher_) {
            setErrorMessage(QStringLiteral("A configuracao de dados nao esta disponivel"));
            return;
        }
        if (pendingLaunchTargets_) {
            launchPendingReplacement();
            return;
        }
        if (!setupPort_) {
            setErrorMessage(QStringLiteral("A configuracao de dados nao esta disponivel"));
            return;
        }
        const auto root = selectedRoot();
        if (root.empty()) {
            setErrorMessage(QStringLiteral("Selecione uma pasta raiz local"));
            return;
        }

        ports::DataSetupRequest request;
        request.action = static_cast<ports::DataSetupAction>(action_);
        request.projectRoot = root;
        if (sourceDatabase_.isLocalFile()) {
            request.sourceDatabase = qt::toFileSystemPath(sourceDatabase_.toLocalFile());
        }
        request.xlsxFiles = xlsxFiles_;

        const auto state = std::make_shared<ExecutionState>();
        executionState_ = state;
        const auto setupPort = setupPort_;
        stopSource_ = std::stop_source{};
        const auto stopToken = stopSource_.get_token();
        setErrorMessage({});
        setProgressMessage(QStringLiteral("Configurando dados..."));
        setRunning(true);
        watcher_.setFuture(
            QtConcurrent::run(QThreadPool::globalInstance(),
                              [state, setupPort, request = std::move(request), stopToken] {
                                  try {
                                      auto result = setupPort->execute(request, stopToken);
                                      const std::scoped_lock lock(state->mutex);
                                      state->result = std::move(result);
                                  } catch (...) {
                                      const std::scoped_lock lock(state->mutex);
                                      state->error = std::current_exception();
                                  }
                              }));
    }

    void DataSetupViewModel::cancel() {
        if (!canCancel()) {
            return;
        }
        canceling_ = true;
        emit stateChanged();
        stopSource_.request_stop();
    }

    void DataSetupViewModel::shutdown() {
        if (shuttingDown_) {
            return;
        }
        shuttingDown_ = true;
        disconnect(&watcher_, nullptr, this, nullptr);
        stopSource_.request_stop();
        executionState_.reset();
        pendingLaunchTargets_.reset();
        running_ = false;
        canceling_ = false;
    }

    std::filesystem::path DataSetupViewModel::selectedRoot() const {
        const auto root =
            destinationMode_ == 0 ? defaultHomeRoot_ : customDestination_.toLocalFile();
        return root.isEmpty() ? std::filesystem::path{} : qt::toFileSystemPath(root);
    }

    void DataSetupViewModel::finishExecution() {
        std::optional<ports::DataSetupResult> result;
        std::exception_ptr error;
        if (executionState_) {
            const std::scoped_lock lock(executionState_->mutex);
            result = std::move(executionState_->result);
            error = executionState_->error;
        }
        executionState_.reset();

        if (result && !result->diagnostic.empty()) {
            qWarning().noquote() << "Data setup diagnostic:"
                                 << QString::fromStdString(result->diagnostic);
        }
        if (canceling_ && (!result || !result->ok)) {
            logAsyncOperationError("Data setup failed after cancellation:", error);
            setRunning(false);
            setErrorMessage({});
            setProgressMessage({});
            return;
        }
        if (error) {
            setRunning(false);
            setProgressMessage({});
            setErrorMessage(QStringLiteral("Falha ao configurar os dados"));
            return;
        }
        if (!result) {
            setRunning(false);
            setProgressMessage({});
            setErrorMessage(QStringLiteral("A configuracao nao retornou resultado"));
            return;
        }
        if (!result->ok) {
            setRunning(false);
            setProgressMessage({});
            setErrorMessage(QString::fromStdString(result->message));
            return;
        }

        const auto projectRoot = selectedRoot();
        pendingLaunchTargets_ = ports::ApplicationLaunchTargets{
            .databasePath = result->databasePath,
            .projectRoot = projectRoot,
            .configDir = projectRoot / "config",
        };
        launchPendingReplacement();
    }

    void DataSetupViewModel::launchPendingReplacement() {
        setErrorMessage({});
        if (!pendingLaunchTargets_) {
            setRunning(false);
            setProgressMessage({});
            setErrorMessage(QStringLiteral("A configuracao nao tem destino para abrir"));
            return;
        }
        const auto launch = launcher_->launchConfigured(*pendingLaunchTargets_);
        setRunning(false);
        setProgressMessage({});
        if (!launch.started) {
            setErrorMessage(QString::fromStdString(launch.message));
            return;
        }
        pendingLaunchTargets_.reset();
        emit replacementStarted();
    }

    void DataSetupViewModel::setRunning(const bool running) {
        if (running_ == running) {
            return;
        }
        running_ = running;
        if (!running_) {
            canceling_ = false;
        }
        emit runningChanged();
        emit stateChanged();
    }

    void DataSetupViewModel::setErrorMessage(QString message) {
        if (errorMessage_ == message) {
            return;
        }
        errorMessage_ = std::move(message);
        emit errorMessageChanged();
    }

    void DataSetupViewModel::setProgressMessage(QString message) {
        if (progressMessage_ == message) {
            return;
        }
        progressMessage_ = std::move(message);
        emit progressMessageChanged();
    }

} // namespace ssa::presentation
