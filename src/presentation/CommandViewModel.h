#pragma once

#include "ports/IExternalCommandPort.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QThreadPool>

#include <memory>

namespace ssa::presentation {

    class CommandViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(QString lastStatus READ lastStatus NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)

      public:
        explicit CommandViewModel(std::shared_ptr<ports::IExternalCommandPort> port,
                                  QThreadPool* commandThreadPool = nullptr,
                                  QObject* parent = nullptr);

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
        void applyResult(const ports::ExternalCommandResult& result);
        void executeCommand(const ports::ExternalCommand& command);
        void setCommandState(QString status, QString message, bool succeeded);

        std::shared_ptr<ports::IExternalCommandPort> port_;
        QThreadPool* commandThreadPool_{nullptr};
        QFutureWatcher<ports::ExternalCommandResult> commandWatcher_;
        QString lastMessage_;
        QString lastStatus_{"idle"};
        bool lastSucceeded_{false};
        bool running_{false};
    };

} // namespace ssa::presentation
