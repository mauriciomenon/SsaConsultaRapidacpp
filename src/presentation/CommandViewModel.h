#pragma once

#include "ports/IExternalCommandPort.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPromise>
#include <QString>

#include <exception>
#include <memory>
#include <mutex>
#include <optional>

namespace ssa::presentation {

    class CommandViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(QString lastStatus READ lastStatus NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)

      public:
        explicit CommandViewModel(std::shared_ptr<ports::IExternalCommandPort> port,
                                  QObject* parent = nullptr);
        ~CommandViewModel() override;

        [[nodiscard]] QString lastMessage() const;
        [[nodiscard]] bool lastSucceeded() const;
        [[nodiscard]] QString lastStatus() const;
        [[nodiscard]] bool running() const;

      signals:
        void lastResultChanged();
        void runningChanged();

      public slots:
        void openSamHome();
        void openSsa(const QString& numeroSsa);
        void openInputFolder();
        void openProcessedFolder();
        void openRedundantFolder();
        void openInstallationGuide();

      private:
        struct ResultState final {
            std::mutex mutex;
            std::optional<ports::ExternalCommandResult> result;
            std::exception_ptr error;
            std::shared_ptr<QPromise<void>> completion;
            bool completed{false};
        };

        void applyResult(const ports::ExternalCommandResult& result);
        void executeCommand(const ports::ExternalCommand& command);
        void finishCommand();
        void setCommandState(QString status, QString message, bool succeeded);
        static void completeTask(const std::shared_ptr<ResultState>& task);

        std::shared_ptr<ports::IExternalCommandPort> port_;
        QFutureWatcher<void> watcher_;
        std::shared_ptr<ResultState> activeTask_;
        QString lastMessage_;
        QString lastStatus_{"idle"};
        bool lastSucceeded_{false};
        bool running_{false};
        bool shuttingDown_{false};
    };

} // namespace ssa::presentation
