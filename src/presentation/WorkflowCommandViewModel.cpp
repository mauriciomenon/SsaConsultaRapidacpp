#include "presentation/WorkflowCommandViewModel.h"

#include <utility>

namespace ssa::presentation {

    WorkflowCommandViewModel::WorkflowCommandViewModel(
        std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent)
        : QObject(parent), runner_(std::move(workflows), this) {
        connect(&runner_, &WorkflowCommandRunner::runningChanged, this,
                &WorkflowCommandViewModel::setRunning);
        connect(&runner_, &WorkflowCommandRunner::finished, this,
                &WorkflowCommandViewModel::applyResult);
    }

    QString WorkflowCommandViewModel::lastMessage() const {
        return lastMessage_;
    }

    bool WorkflowCommandViewModel::lastSucceeded() const {
        return lastSucceeded_;
    }

    bool WorkflowCommandViewModel::running() const {
        return running_;
    }

    void WorkflowCommandViewModel::rescanIncremental() {
        startRescan(ports::RescanMode::Incremental);
    }

    void WorkflowCommandViewModel::rescanFull() {
        startRescan(ports::RescanMode::Full);
    }

    void WorkflowCommandViewModel::startRescan(const ports::RescanMode mode) {
        if (runner_.running()) {
            return;
        }
        runner_.rescan(mode);
    }

    void WorkflowCommandViewModel::applyResult(const ports::WorkflowResult& result) {
        setResult(QString::fromStdString(result.message), result.ok());
    }

    void WorkflowCommandViewModel::setRunning(const bool running) {
        if (running_ == running) {
            return;
        }
        running_ = running;
        emit runningChanged();
    }

    void WorkflowCommandViewModel::setResult(QString message, const bool succeeded) {
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
