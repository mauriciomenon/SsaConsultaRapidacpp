#include "presentation/WorkflowCommandRunner.h"

#include <QThreadPool>
#include <QtConcurrent>

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
        disconnect(&watcher_, nullptr, this, nullptr);
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
        }
        resultState_.reset();
    }

    bool WorkflowCommandRunner::running() const {
        return running_;
    }

    void WorkflowCommandRunner::importExternalFiles(const std::vector<QString>& files) {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit this->finished({ports::WorkflowStatus::Failed,
                                 "import_external_files workflow is not configured"});
            return;
        }

        ports::ImportExternalFilesRequest request;
        request.optimized = true;
        request.files.reserve(files.size());
        for (const auto& path : files) {
            request.files.emplace_back(path.toStdString());
        }
        auto workflows = workflows_;
        start([workflows = std::move(workflows), request = std::move(request)] {
            return workflows->importExternalFiles(request);
        });
    }

    void WorkflowCommandRunner::rescan(const ports::RescanMode mode) {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "rescan workflow is not configured"});
            return;
        }

        ports::RescanRequest request;
        request.mode = mode;
        request.allowFileDiscovery = true;
        request.optimized = mode == ports::RescanMode::Incremental;

        auto workflows = workflows_;
        start([workflows = std::move(workflows), request] { return workflows->rescan(request); });
    }

    void WorkflowCommandRunner::syncDerivadas() {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "sync derivadas workflow is not configured"});
            return;
        }

        auto workflows = workflows_;
        start([workflows = std::move(workflows)] { return workflows->syncDerivadas(); });
    }

    void WorkflowCommandRunner::compactDatabase() {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit this->finished(
                {ports::WorkflowStatus::Failed, "compact database workflow is not configured"});
            return;
        }

        auto workflows = workflows_;
        start([workflows = std::move(workflows)] { return workflows->vacuumAnalyze(); });
    }

    void WorkflowCommandRunner::start(std::function<ports::WorkflowResult()> operation) {
        running_ = true;
        emit this->runningChanged(running_);
        const auto state = std::make_shared<ResultState>();
        resultState_ = state;
        watcher_.setFuture(QtConcurrent::run(QThreadPool::globalInstance(),
                                             [state, operation = std::move(operation)] {
                                                 try {
                                                     auto result = operation();
                                                     const std::scoped_lock lock(state->mutex);
                                                     state->result = std::move(result);
                                                 } catch (...) {
                                                     const std::scoped_lock lock(state->mutex);
                                                     state->error = std::current_exception();
                                                 }
                                             }));
    }

    void WorkflowCommandRunner::finish() {
        std::optional<ports::WorkflowResult> result;
        std::exception_ptr error;
        if (resultState_) {
            const std::scoped_lock lock(resultState_->mutex);
            result = std::move(resultState_->result);
            error = resultState_->error;
        }
        resultState_.reset();
        running_ = false;
        emit this->runningChanged(running_);
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& exception) {
                emit this->finished({ports::WorkflowStatus::Failed, exception.what()});
            } catch (...) {
                emit this->finished({ports::WorkflowStatus::Failed, "unknown workflow error"});
            }
            return;
        }
        if (!result) {
            emit this->finished({ports::WorkflowStatus::Failed, "workflow produced no result"});
            return;
        }
        emit this->finished(std::move(*result));
    }

} // namespace ssa::presentation
