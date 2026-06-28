#include "presentation/CommandViewModel.h"

#include <QThreadPool>
#include <QtConcurrent>

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {

        QString statusName(const ports::ExternalCommandStatus status) {
            switch (status) {
            case ports::ExternalCommandStatus::Succeeded:
                return "succeeded";
            case ports::ExternalCommandStatus::NotImplemented:
                return "not_implemented";
            case ports::ExternalCommandStatus::Rejected:
                return "rejected";
            case ports::ExternalCommandStatus::Failed:
                return "failed";
            }
            // Throw in all builds: a new enum value must not silently fall through
            // (Q_ASSERT_X is compiled out in release).
            throw std::logic_error("unhandled ExternalCommandStatus");
        }

    } // namespace

    CommandViewModel::CommandViewModel(std::shared_ptr<ports::IExternalCommandPort> port,
                                       QThreadPool* commandThreadPool, QObject* parent)
        : QObject(parent), port_(std::move(port)),
          commandThreadPool_(commandThreadPool != nullptr ? commandThreadPool
                                                          : QThreadPool::globalInstance()) {
        if (!port_) {
            throw std::invalid_argument("external command port is required");
        }
        connect(&commandWatcher_, &QFutureWatcher<ports::ExternalCommandResult>::finished, this,
                [this] { applyResult(commandWatcher_.result()); });
    }

    QString CommandViewModel::lastMessage() const {
        return lastMessage_;
    }

    bool CommandViewModel::lastSucceeded() const {
        return lastSucceeded_;
    }

    QString CommandViewModel::lastStatus() const {
        return lastStatus_;
    }

    bool CommandViewModel::running() const {
        return running_;
    }

    void CommandViewModel::openSamHome() {
        executeCommand({ports::ExternalCommandKind::OpenSamHome});
    }

    void CommandViewModel::openSsa(const QString& numeroSsa) {
        ports::ExternalCommand command;
        command.kind = ports::ExternalCommandKind::OpenSsa;
        command.parameters.emplace("ssa_number", numeroSsa.toStdString());
        executeCommand(command);
    }

    void CommandViewModel::openInputFolder() {
        executeCommand({ports::ExternalCommandKind::OpenInputFolder});
    }

    void CommandViewModel::openProcessedFolder() {
        executeCommand({ports::ExternalCommandKind::OpenProcessedFolder});
    }

    void CommandViewModel::openRedundantFolder() {
        executeCommand({ports::ExternalCommandKind::OpenRedundantFolder});
    }

    void CommandViewModel::openInstallationGuide() {
        executeCommand({ports::ExternalCommandKind::OpenInstallationGuide});
    }

    void CommandViewModel::applyResult(const ports::ExternalCommandResult& result) {
        if (running_) {
            running_ = false;
            emit runningChanged();
        }
        setCommandState(statusName(result.status), QString::fromStdString(result.message),
                        result.ok());
    }

    void CommandViewModel::executeCommand(const ports::ExternalCommand& command) {
        if (commandWatcher_.isRunning()) {
            return;
        }
        running_ = true;
        emit runningChanged();

        const std::weak_ptr<ports::IExternalCommandPort> weakPort = port_;
        commandWatcher_.setFuture(QtConcurrent::run(commandThreadPool_, [weakPort, command] {
            const auto port = weakPort.lock();
            if (!port) {
                return ports::ExternalCommandResult{ports::ExternalCommandStatus::Failed,
                                                    "external command port is unavailable"};
            }
            return port->execute(command);
        }));
    }

    void CommandViewModel::setCommandState(QString status, QString message, const bool succeeded) {
        lastStatus_ = std::move(status);
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
