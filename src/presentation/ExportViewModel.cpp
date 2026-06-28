#include "presentation/ExportViewModel.h"

#include <QtConcurrent>

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {

        QString statusName(const ports::WorkflowStatus status) {
            switch (status) {
            case ports::WorkflowStatus::Succeeded:
                return "succeeded";
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
            ownedExportThreadPool_ = std::make_unique<QThreadPool>();
            ownedExportThreadPool_->setMaxThreadCount(1);
            exportThreadPool_ = ownedExportThreadPool_.get();
        }
        connect(&exportWatcher_, &QFutureWatcher<ports::WorkflowResult>::finished, this,
                [this] { applyResult(exportWatcher_.result()); });
    }

    ExportViewModel::~ExportViewModel() {
        exportWatcher_.waitForFinished();
        if (ownedExportThreadPool_) {
            ownedExportThreadPool_->waitForDone();
        }
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
        request.outputPath = std::filesystem::path{outputUrl.toLocalFile().toStdString()};
        request.query = requestFactory_();
        request.query.pageIndex = 0;

        running_ = true;
        emit runningChanged();

        const std::shared_ptr<application::SsaWorkflowService> workflows = workflows_;
        exportWatcher_.setFuture(QtConcurrent::run(exportThreadPool_, [workflows, request] {
            return workflows->exportFilteredList(request);
        }));
    }

    void ExportViewModel::applyResult(const ports::WorkflowResult& result) {
        if (running_) {
            running_ = false;
            emit runningChanged();
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
