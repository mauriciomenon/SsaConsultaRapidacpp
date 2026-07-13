#include "application/SsaWorkflowService.h"

#include <utility>

namespace ssa::application {

    namespace {

        ports::WorkflowResult withCleanupStatus(ports::WorkflowResult result,
                                                const bool cleanupSucceeded) {
            if (cleanupSucceeded) {
                return result;
            }
            result.warning = true;
            result.message += "; temporary SAM artifacts could not be removed";
            if (!result.diagnostic.empty()) {
                result.diagnostic += "; ";
            }
            result.diagnostic += "SAM artifact cleanup failed";
            return result;
        }

    } // namespace

    SsaWorkflowService::SsaWorkflowService(
        std::shared_ptr<ports::IImportWorkflowPort> importPort,
        std::shared_ptr<ports::IExportPort> exportPort,
        std::shared_ptr<ports::IDatabaseMaintenancePort> maintenancePort,
        std::shared_ptr<ports::IDerivadasPort> derivadasPort,
        std::shared_ptr<ports::ISamRefreshPort> samPort)
        : importPort_(std::move(importPort)), exportPort_(std::move(exportPort)),
          maintenancePort_(std::move(maintenancePort)), derivadasPort_(std::move(derivadasPort)),
          samPort_(std::move(samPort)) {}

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

    ports::WorkflowResult SsaWorkflowService::refreshSam(const ports::SamRefreshRequest& request,
                                                         const std::stop_token& stopToken) const {
        if (!samPort_) {
            return notImplemented("SAM refresh");
        }

        auto fetchResult = samPort_->fetch(request, stopToken);
        if (!fetchResult.ok()) {
            return withCleanupStatus({fetchResult.status, std::move(fetchResult.message), false,
                                      std::move(fetchResult.diagnostic)},
                                     samPort_->discardArtifacts());
        }
        if (!importPort_) {
            return withCleanupStatus(notImplemented("SAM import"), samPort_->discardArtifacts());
        }

        auto result = importPort_->importExternalFiles(
            {.files = std::move(fetchResult.artifacts), .optimized = true}, stopToken);
        return withCleanupStatus(std::move(result), samPort_->discardArtifacts());
    }

    ports::WorkflowResult SsaWorkflowService::notImplemented(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
