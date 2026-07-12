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

    ports::WorkflowResult
    SsaWorkflowService::importExternalFiles(const ports::ImportExternalFilesRequest& request,
                                            std::stop_token stopToken) const {
        if (!importPort_) {
            return notImplemented("import external files");
        }
        return importPort_->importExternalFiles(request, std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::rescan(const ports::RescanRequest& request,
                                                     std::stop_token stopToken) const {
        if (!importPort_) {
            return notImplemented("rescan");
        }
        return importPort_->rescan(request, std::move(stopToken));
    }

    ports::WorkflowResult
    SsaWorkflowService::exportFilteredList(const ports::ExportFilteredListRequest& request,
                                           std::stop_token stopToken) const {
        if (!exportPort_) {
            return notImplemented("export filtered list");
        }
        return exportPort_->exportFilteredList(request, std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::resetDatabase(std::stop_token stopToken) const {
        if (!maintenancePort_) {
            return notImplemented("reset database");
        }
        return maintenancePort_->resetDatabase(std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::cleanData(std::stop_token stopToken) const {
        if (!maintenancePort_) {
            return notImplemented("clean data");
        }
        return maintenancePort_->cleanData(std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::vacuumAnalyze(std::stop_token stopToken) const {
        if (!maintenancePort_) {
            return notImplemented("vacuum analyze");
        }
        return maintenancePort_->vacuumAnalyze(std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::syncDerivadas(std::stop_token stopToken) const {
        if (!derivadasPort_) {
            return notImplemented("sync derivadas");
        }
        return derivadasPort_->syncDerivadas(std::move(stopToken));
    }

    ports::WorkflowResult SsaWorkflowService::notImplemented(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
