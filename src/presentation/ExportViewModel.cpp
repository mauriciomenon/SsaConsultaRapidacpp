#include "presentation/ExportViewModel.h"

#include "qt/FilesystemPath.h"

#include <QThreadPool>
#include <QtConcurrentRun>

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {

        QString statusName(const ports::WorkflowStatus status) {
            switch (status) {
            case ports::WorkflowStatus::Succeeded:
                return "succeeded";
            case ports::WorkflowStatus::Canceled:
                return "canceled";
            case ports::WorkflowStatus::NotImplemented:
                return "not_implemented";
            case ports::WorkflowStatus::Rejected:
                return "rejected";
            case ports::WorkflowStatus::Failed:
                return "failed";
            }
            // Throw in all builds: a new enum value must not silently fall through
            // (Q_ASSERT_X is compiled out in release).
            throw std::logic_error("unhandled WorkflowStatus");
        }

    } // namespace

    ExportViewModel::ExportViewModel(std::shared_ptr<application::SsaWorkflowService> workflows,
                                     RequestFactory requestFactory, QThreadPool* exportThreadPool,
                                     QObject* parent)
        : QObject(parent), workflows_(std::move(workflows)),
          requestFactory_(std::move(requestFactory)) {
        if (!requestFactory_) {
            throw std::invalid_argument("export request factory is required");
        }
        if (exportThreadPool != nullptr) {
            exportThreadPool_ = exportThreadPool;
        } else {
            exportThreadPool_ = QThreadPool::globalInstance();
        }
        connect(&exportWatcher_, &QFutureWatcher<void>::finished, this,
                &ExportViewModel::finishExport);
    }

    ExportViewModel::~ExportViewModel() {
        disconnect(&exportWatcher_, nullptr, this, nullptr);
        exportStopSource_.request_stop();
        exportWatcher_.cancel();
        resultState_.reset();
    }

    QString ExportViewModel::lastMessage() const {
        return lastMessage_;
    }

    bool ExportViewModel::lastSucceeded() const {
        return lastSucceeded_;
    }

    QString ExportViewModel::lastStatus() const {
        return lastStatus_;
    }

    bool ExportViewModel::running() const {
        return running_;
    }

    bool ExportViewModel::canceling() const {
        return canceling_;
    }

    bool ExportViewModel::canCancel() const {
        return running_ && !canceling_;
    }

    void ExportViewModel::exportFilteredList(const QUrl& outputUrl) {
        if (exportWatcher_.isRunning()) {
            return;
        }
        if (!workflows_) {
            setExportState("not_implemented", "export workflow is not configured", false);
            return;
        }
        if (!outputUrl.isLocalFile()) {
            setExportState("rejected", "export output path must be a local file", false);
            return;
        }

        ports::ExportFilteredListRequest request;
        request.outputPath = qt::toFileSystemPath(outputUrl.toLocalFile());
        request.query = requestFactory_();
        request.query.pageIndex = 0;

        running_ = true;
        emit runningChanged();
        emit stateChanged();

        const std::shared_ptr<application::SsaWorkflowService> workflows = workflows_;
        const auto state = std::make_shared<ResultState>();
        resultState_ = state;
        exportStopSource_ = std::stop_source{};
        const auto stopToken = exportStopSource_.get_token();
        exportWatcher_.setFuture(
            QtConcurrent::run(exportThreadPool_, [workflows, request, state, stopToken] {
                try {
                    auto result = workflows->exportFilteredList(request, stopToken);
                    const std::scoped_lock lock(state->mutex);
                    state->result = std::move(result);
                } catch (...) {
                    const std::scoped_lock lock(state->mutex);
                    state->error = std::current_exception();
                }
            }));
    }

    void ExportViewModel::cancel() {
        if (!canCancel()) {
            return;
        }
        canceling_ = true;
        emit stateChanged();
        exportStopSource_.request_stop();
    }

    void ExportViewModel::finishExport() {
        std::optional<ports::WorkflowResult> result;
        std::exception_ptr error;
        if (resultState_) {
            const std::scoped_lock lock(resultState_->mutex);
            result = std::move(resultState_->result);
            error = resultState_->error;
        }
        resultState_.reset();
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& exception) {
                applyResult({ports::WorkflowStatus::Failed, exception.what()});
            } catch (...) {
                applyResult({ports::WorkflowStatus::Failed, "unknown export error"});
            }
            return;
        }
        if (!result) {
            applyResult({ports::WorkflowStatus::Failed, "export produced no result"});
            return;
        }
        applyResult(*result);
    }

    void ExportViewModel::applyResult(const ports::WorkflowResult& result) {
        if (running_) {
            running_ = false;
            canceling_ = false;
            emit runningChanged();
            emit stateChanged();
        }
        setExportState(statusName(result.status), QString::fromStdString(result.message),
                       result.ok());
    }

    void ExportViewModel::setExportState(QString status, QString message, const bool succeeded) {
        lastStatus_ = std::move(status);
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
