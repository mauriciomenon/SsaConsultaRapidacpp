#pragma once

#include "ports/IDatabaseSwitchPorts.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::presentation {

    class DataSetupViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)
        Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
        Q_PROPERTY(QString destinationPath READ destinationPath NOTIFY selectionChanged)
        Q_PROPERTY(int action READ action NOTIFY selectionChanged)
        Q_PROPERTY(int destinationMode READ destinationMode NOTIFY selectionChanged)

      public:
        DataSetupViewModel(std::shared_ptr<ports::IDataSetupPort> setupPort,
                           std::shared_ptr<ports::IApplicationLauncher> launcher,
                           QString defaultHomeRoot, QObject* parent = nullptr);
        ~DataSetupViewModel() override;

        [[nodiscard]] bool running() const;
        [[nodiscard]] bool canCancel() const;
        [[nodiscard]] bool canceling() const;
        [[nodiscard]] QString errorMessage() const;
        [[nodiscard]] QString progressMessage() const;
        [[nodiscard]] QString destinationPath() const;
        [[nodiscard]] int action() const;
        [[nodiscard]] int destinationMode() const;

        Q_INVOKABLE void setAction(int action);
        Q_INVOKABLE void setDestinationMode(int mode);
        Q_INVOKABLE void setCustomDestination(const QUrl& url);
        Q_INVOKABLE void setSourceDatabase(const QUrl& url);
        Q_INVOKABLE void setXlsxFiles(const QVariantList& files);
        Q_INVOKABLE void execute();
        Q_INVOKABLE void cancel();
        Q_INVOKABLE void shutdown();

      signals:
        void runningChanged();
        void stateChanged();
        void errorMessageChanged();
        void progressMessageChanged();
        void selectionChanged();
        void replacementStarted();

      private:
        struct ExecutionState final {
            std::mutex mutex;
            std::optional<ports::DataSetupResult> result;
            std::exception_ptr error;
        };

        [[nodiscard]] std::filesystem::path selectedRoot() const;
        void finishExecution();
        void launchPendingReplacement();
        void setRunning(bool running);
        void setErrorMessage(QString message);
        void setProgressMessage(QString message);

        std::shared_ptr<ports::IDataSetupPort> setupPort_;
        std::shared_ptr<ports::IApplicationLauncher> launcher_;
        QString defaultHomeRoot_;
        QUrl customDestination_;
        QUrl sourceDatabase_;
        std::vector<std::filesystem::path> xlsxFiles_;
        QFutureWatcher<void> watcher_;
        std::shared_ptr<ExecutionState> executionState_;
        std::optional<ports::ApplicationLaunchTargets> pendingLaunchTargets_;
        std::stop_source stopSource_;
        QString errorMessage_;
        QString progressMessage_;
        int action_ = 0;
        int destinationMode_ = 0;
        bool running_ = false;
        bool canceling_ = false;
        bool shuttingDown_ = false;
    };

} // namespace ssa::presentation
