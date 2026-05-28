#include "presentation/WorkflowCommandRunner.h"

#include <QThreadPool>
#include <QtConcurrent>

#include <filesystem>
#include <utility>

namespace ssa::presentation {

    WorkflowCommandRunner::WorkflowCommandRunner(
        std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent)
        : QObject(parent), workflows_(std::move(workflows)) {
        connect(&watcher_, &QFutureWatcher<ports::WorkflowResult>::finished, this,
                &WorkflowCommandRunner::finish);
    }

    bool WorkflowCommandRunner::running() const {
        return running_;
    }

    void WorkflowCommandRunner::importExternalFiles(std::vector<QString> files) {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit finished({ports::WorkflowStatus::Failed,
                           "import_external_files workflow is not configured"});
            return;
        }

        ports::ImportExternalFilesRequest request;
        request.optimized = true;
        request.files.reserve(files.size());
        for (auto& path : files) {
            request.files.emplace_back(std::filesystem::path(path.toStdString()));
        }
        running_ = true;
        emit runningChanged(running_);

        auto workflows = workflows_;
        watcher_.setFuture(
            QtConcurrent::run(QThreadPool::globalInstance(),
                              [workflows = std::move(workflows), request = std::move(request)] {
                                  return workflows->importExternalFiles(request);
                              }));
    }

    void WorkflowCommandRunner::rescan(const ports::RescanMode mode) {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit finished({ports::WorkflowStatus::Failed, "rescan workflow is not configured"});
            return;
        }

        ports::RescanRequest request;
        request.mode = mode;
        request.allowFileDiscovery = true;
        request.optimized = mode == ports::RescanMode::Incremental;

        running_ = true;
        emit runningChanged(running_);

        auto workflows = workflows_;
        watcher_.setFuture(
            QtConcurrent::run(QThreadPool::globalInstance(),
                              [workflows = std::move(workflows), request = std::move(request)] {
                                  return workflows->rescan(request);
                              }));
    }

    void WorkflowCommandRunner::syncDerivadas() {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit finished(
                {ports::WorkflowStatus::Failed, "sync derivadas workflow is not configured"});
            return;
        }

        running_ = true;
        emit runningChanged(running_);

        auto workflows = workflows_;
        watcher_.setFuture(
            QtConcurrent::run(QThreadPool::globalInstance(), [workflows = std::move(workflows)] {
                return workflows->syncDerivadas();
            }));
    }

    void WorkflowCommandRunner::finish() {
        const ports::WorkflowResult result = watcher_.result();
        running_ = false;
        emit runningChanged(running_);
        emit finished(result);
    }

} // namespace ssa::presentation
