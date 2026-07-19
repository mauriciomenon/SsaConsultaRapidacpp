#include "presentation/WorkflowCommandRunner.h"

#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QDebug>
#include <QThreadPool>
#include <QtConcurrentRun>

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    WorkflowCommandRunner::WorkflowCommandRunner(
        std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent)
        : QObject(parent), workflows_(std::move(workflows)) {
        connect(&watcher_, &QFutureWatcher<void>::finished, this, &WorkflowCommandRunner::finish);
    }

    WorkflowCommandRunner::~WorkflowCommandRunner() {
        shutdown();
    }

    void WorkflowCommandRunner::shutdown() {
        if (shuttingDown_) {
            return;
        }
        shuttingDown_ = true;
        if (running()) {
            setState(State::Canceling);
        }
        stopSource_.request_stop();
    }

    WorkflowCommandRunner::State WorkflowCommandRunner::state() const {
        return state_;
    }

    bool WorkflowCommandRunner::running() const {
        return state_ != State::Idle;
    }

    bool WorkflowCommandRunner::canceling() const {
        return state_ == State::Canceling;
    }

    bool WorkflowCommandRunner::canCancel() const {
        return state_ == State::Running;
    }

    bool WorkflowCommandRunner::legacySpreadsheetConverterAvailable() const {
        return workflows_ && workflows_->legacySpreadsheetConverterAvailable();
    }

    void WorkflowCommandRunner::importExternalFiles(const std::vector<QString>& files,
                                                    ports::ImportExecutionOptions execution) {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished({ports::WorkflowStatus::Failed,
                                 "import_external_files workflow is not configured"});
            return;
        }

        ports::ImportExternalFilesRequest request;
        request.execution = execution;
        request.files.reserve(files.size());
        for (const auto& path : files) {
            request.files.emplace_back(qt::toFileSystemPath(path));
        }
        auto workflows = workflows_;
        start([workflows = std::move(workflows),
               request = std::move(request)](const std::stop_token& stopToken) {
            return workflows->importExternalFiles(request, stopToken);
        });
    }

    void WorkflowCommandRunner::importDerivations(const std::vector<QString>& files) {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "derivadas import workflow is not configured"});
            return;
        }

        ports::ImportDerivationsRequest request;
        request.files.reserve(files.size());
        for (const auto& path : files) {
            request.files.emplace_back(qt::toFileSystemPath(path));
        }
        auto workflows = workflows_;
        start([workflows = std::move(workflows),
               request = std::move(request)](const std::stop_token& stopToken) {
            return workflows->importDerivations(request, stopToken);
        });
    }

    void WorkflowCommandRunner::rescan(const ports::RescanMode mode,
                                       ports::ImportExecutionOptions execution) {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "rescan workflow is not configured"});
            return;
        }

        ports::RescanRequest request;
        request.mode = mode;
        request.execution = execution;

        auto workflows = workflows_;
        start([workflows = std::move(workflows), request](const std::stop_token& stopToken) {
            return workflows->rescan(request, stopToken);
        });
    }

    void WorkflowCommandRunner::refreshSam(ports::SamRefreshRequest request) {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "SAM refresh workflow is not configured"});
            return;
        }

        auto workflows = workflows_;
        start([workflows = std::move(workflows),
               request = std::move(request)](const std::stop_token& stopToken) {
            return workflows->refreshSam(request, stopToken);
        });
    }

    void WorkflowCommandRunner::cleanOrphanDerivations() {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished({ports::WorkflowStatus::Failed,
                                 "orphan derivation cleanup workflow is not configured"});
            return;
        }

        auto workflows = workflows_;
        start([workflows = std::move(workflows)](const std::stop_token& stopToken) {
            return workflows->cleanOrphanDerivations(stopToken);
        });
    }

    void WorkflowCommandRunner::compactDatabase() {
        if (running() || shuttingDown_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "compact database workflow is not configured"});
            return;
        }

        auto workflows = workflows_;
        start([workflows = std::move(workflows)](const std::stop_token& stopToken) {
            return workflows->vacuumAnalyze(stopToken);
        });
    }

    void
    WorkflowCommandRunner::start(std::function<ports::WorkflowResult(std::stop_token)> operation) {
        const auto state = std::make_shared<ResultState>();
        resultState_ = state;
        stopSource_ = std::stop_source{};
        const auto stopToken = stopSource_.get_token();
        setState(State::Running);
        watcher_.setFuture(QtConcurrent::run(QThreadPool::globalInstance(),
                                             [state, operation = std::move(operation), stopToken] {
                                                 try {
                                                     auto result = operation(stopToken);
                                                     const std::scoped_lock lock(state->mutex);
                                                     state->result = std::move(result);
                                                 } catch (...) {
                                                     const std::scoped_lock lock(state->mutex);
                                                     state->error = std::current_exception();
                                                 }
                                             }));
    }

    void WorkflowCommandRunner::cancel() {
        if (!canCancel() || shuttingDown_) {
            return;
        }
        setState(State::Canceling);
        stopSource_.request_stop();
    }

    void WorkflowCommandRunner::finish() {
        std::optional<ports::WorkflowResult> result = std::nullopt;
        std::exception_ptr error;
        if (resultState_) {
            const std::scoped_lock lock(resultState_->mutex);
            result = std::move(resultState_->result);
            error = resultState_->error;
        }
        resultState_.reset();
        if (shuttingDown_) {
            setState(State::Idle);
            return;
        }
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (const ports::OperationError& exception) {
                qWarning().noquote()
                    << "Workflow failed:" << QString::fromStdString(exception.diagnostic());
                emit this->finished({ports::WorkflowStatus::Failed, exception.what()});
            } catch (const std::exception& exception) {
                qWarning().noquote() << "Workflow failed:" << exception.what();
                emit this->finished(
                    {ports::WorkflowStatus::Failed, "Falha ao executar a operacao"});
            } catch (...) {
                qWarning() << "Workflow failed: unknown exception";
                emit this->finished(
                    {ports::WorkflowStatus::Failed, "Falha ao executar a operacao"});
            }
        } else if (!result) {
            emit this->finished({ports::WorkflowStatus::Failed, "workflow produced no result"});
        } else {
            if (!result->diagnostic.empty()) {
                qWarning().noquote()
                    << "Workflow diagnostic:" << QString::fromStdString(result->diagnostic);
            }
            emit this->finished(std::move(*result));
        }
        setState(State::Idle);
    }

    void WorkflowCommandRunner::setState(const State state) {
        if (state_ == state) {
            return;
        }
        const bool wasRunning = running();
        state_ = state;
        emit stateChanged(state_);
        if (wasRunning != running()) {
            emit runningChanged(running());
        }
    }

} // namespace ssa::presentation
