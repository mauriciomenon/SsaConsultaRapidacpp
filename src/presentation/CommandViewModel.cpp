#include "presentation/CommandViewModel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

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
                                       QObject* parent)
        : QObject(parent), port_(std::move(port)) {
        if (!port_) {
            throw std::invalid_argument("external command port is required");
        }
        connect(&watcher_, &QFutureWatcher<void>::finished, this, &CommandViewModel::finishCommand);
    }

    CommandViewModel::~CommandViewModel() {
        shuttingDown_ = true;
        disconnect(&watcher_, nullptr, this, nullptr);
        completeTask(activeTask_);
        activeTask_.reset();
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
        if (running_ || shuttingDown_) {
            return;
        }
        if (thread() != QThread::currentThread() || QCoreApplication::instance() == nullptr ||
            QCoreApplication::instance()->thread() != QThread::currentThread()) {
            qWarning() << "CommandViewModel executeCommand called outside GUI thread";
            return;
        }
        running_ = true;
        emit runningChanged();
        const auto task = std::make_shared<ResultState>();
        task->completion = std::make_shared<QPromise<void>>();
        task->completion->start();
        activeTask_ = task;
        watcher_.setFuture(task->completion->future());

        const auto port = port_;
        QTimer::singleShot(0, this, [port, command, task] {
            try {
                const auto result = port->execute(command);
                const std::scoped_lock lock(task->mutex);
                task->result = result;
            } catch (...) {
                const std::scoped_lock lock(task->mutex);
                task->error = std::current_exception();
            }
            completeTask(task);
        });
    }

    void CommandViewModel::finishCommand() {
        if (shuttingDown_ || !activeTask_) {
            return;
        }
        std::optional<ports::ExternalCommandResult> result;
        std::exception_ptr error;
        {
            const std::scoped_lock lock(activeTask_->mutex);
            result = activeTask_->result;
            error = activeTask_->error;
        }
        activeTask_.reset();
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& exc) {
                applyResult({ports::ExternalCommandStatus::Failed, exc.what()});
            } catch (...) {
                applyResult(
                    {ports::ExternalCommandStatus::Failed, "unknown external command error"});
            }
            return;
        }
        if (!result) {
            applyResult(
                {ports::ExternalCommandStatus::Failed, "external command produced no result"});
            return;
        }
        applyResult(*result);
    }

    void CommandViewModel::completeTask(const std::shared_ptr<ResultState>& task) {
        if (!task) {
            return;
        }
        std::shared_ptr<QPromise<void>> completion;
        {
            const std::scoped_lock lock(task->mutex);
            if (task->completed) {
                return;
            }
            task->completed = true;
            completion = task->completion;
        }
        completion->finish();
    }

    void CommandViewModel::setCommandState(QString status, QString message, const bool succeeded) {
        lastStatus_ = std::move(status);
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
