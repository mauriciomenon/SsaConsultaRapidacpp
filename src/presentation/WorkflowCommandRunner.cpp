#include "presentation/WorkflowCommandRunner.h"

#include <QThreadPool>
#include <QtConcurrent>

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

    void WorkflowCommandRunner::rescan(const ports::RescanMode mode) {
        if (running_) {
            return;
        }
        if (!workflows_) {
            emit finished(
                {ports::WorkflowStatus::NotImplemented, "rescan workflow is not configured"});
            return;
        }

        ports::RescanRequest request;
        request.mode = mode;
        request.allowFileDiscovery = true;
        request.optimized = mode == ports::RescanMode::Incremental;

        running_ = true;
        emit runningChanged(running_);

        const std::shared_ptr<application::SsaWorkflowService> workflows = workflows_;
        watcher_.setFuture(QtConcurrent::run(QThreadPool::globalInstance(), [workflows, request] {
            return workflows->rescan(request);
        }));
    }

    void WorkflowCommandRunner::finish() {
        const ports::WorkflowResult result = watcher_.result();
        running_ = false;
        emit runningChanged(running_);
        emit finished(result);
    }

} // namespace ssa::presentation
