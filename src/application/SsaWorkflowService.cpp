#include "application/SsaWorkflowService.h"

#include <utility>

namespace ssa::application {

    SsaWorkflowService::SsaWorkflowService(
        std::shared_ptr<ports::IImportWorkflowPort> importPort,
        std::shared_ptr<ports::IExportPort> exportPort,
        std::shared_ptr<ports::IDatabaseMaintenancePort> maintenancePort,
        std::shared_ptr<ports::IDerivadasPort> derivadasPort)
        : importPort_(std::move(importPort)), exportPort_(std::move(exportPort)),
          maintenancePort_(std::move(maintenancePort)), derivadasPort_(std::move(derivadasPort)) {}

    ports::WorkflowResult SsaWorkflowService::importExternalFiles(
        const ports::ImportExternalFilesRequest& request) const {
        if (!importPort_) {
            return notImplemented("import external files");
        }
        return importPort_->importExternalFiles(request);
    }

    ports::WorkflowResult SsaWorkflowService::rescan(const ports::RescanRequest& request) const {
        if (!importPort_) {
            return notImplemented("rescan");
        }
        return importPort_->rescan(request);
    }

    ports::WorkflowResult
    SsaWorkflowService::exportFilteredList(const ports::ExportFilteredListRequest& request,
                                           std::stop_token stopToken) const {
        if (!exportPort_) {
            return notImplemented("export filtered list");
        }
        return exportPort_->exportFilteredList(request, std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::resetDatabase() const {
        if (!maintenancePort_) {
            return notImplemented("reset database");
        }
        return maintenancePort_->resetDatabase();
    }

    ports::WorkflowResult SsaWorkflowService::cleanData() const {
        if (!maintenancePort_) {
            return notImplemented("clean data");
        }
        return maintenancePort_->cleanData();
    }

    ports::WorkflowResult SsaWorkflowService::vacuumAnalyze() const {
        if (!maintenancePort_) {
            return notImplemented("vacuum analyze");
        }
        return maintenancePort_->vacuumAnalyze();
    }

    ports::WorkflowResult SsaWorkflowService::syncDerivadas() const {
        if (!derivadasPort_) {
            return notImplemented("sync derivadas");
        }
        return derivadasPort_->syncDerivadas();
    }

    ports::WorkflowResult SsaWorkflowService::notImplemented(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
