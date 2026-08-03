#include "application/SsaWorkflowService.h"

#include "domain/SsaImportPolicy.h"

#include <utility>
#include <vector>

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

        ports::WorkflowResult mergeDerivadasOutcome(ports::WorkflowResult regularResult,
                                                    const ports::WorkflowResult& derivadasResult) {
            if (derivadasResult.ok()) {
                regularResult.warning = regularResult.warning || derivadasResult.warning;
                if (derivadasResult.warning) {
                    if (!regularResult.diagnostic.empty()) {
                        regularResult.diagnostic += "; ";
                    }
                    regularResult.diagnostic += derivadasResult.message;
                    if (!derivadasResult.diagnostic.empty()) {
                        regularResult.diagnostic += "; " + derivadasResult.diagnostic;
                    }
                }
                return regularResult;
            }
            regularResult.warning = true;
            if (!regularResult.diagnostic.empty()) {
                regularResult.diagnostic += "; ";
            }
            regularResult.diagnostic += derivadasResult.message;
            if (!derivadasResult.diagnostic.empty()) {
                regularResult.diagnostic += "; " + derivadasResult.diagnostic;
            }
            regularResult.message += "; derivadas: " + derivadasResult.message;
            return regularResult;
        }

    } // namespace

    SsaWorkflowService::SsaWorkflowService(
        std::shared_ptr<ports::IImportWorkflowPort> importPort,
        std::shared_ptr<ports::IExportPort> exportPort,
        std::shared_ptr<ports::IDatabaseMaintenancePort> maintenancePort,
        std::shared_ptr<ports::IDerivadasPort> derivadasPort,
        std::shared_ptr<ports::ISamRefreshPort> samPort,
        std::shared_ptr<ports::ISamImportPort> samImportPort)
        : importPort_(std::move(importPort)), exportPort_(std::move(exportPort)),
          maintenancePort_(std::move(maintenancePort)), derivadasPort_(std::move(derivadasPort)),
          samPort_(std::move(samPort)), samImportPort_(std::move(samImportPort)) {}

    ports::WorkflowResult
    SsaWorkflowService::importExternalFiles(const ports::ImportExternalFilesRequest& request,
                                            std::stop_token stopToken) const {
        std::vector<std::filesystem::path> regularFiles;
        std::vector<std::filesystem::path> derivadasFiles;
        regularFiles.reserve(request.files.size());
        derivadasFiles.reserve(request.files.size());
        for (const auto& file : request.files) {
            if (domain::SsaImportPolicy::classifySourceProfile(file.filename().string()) ==
                domain::SsaImportPolicy::SourceProfile::DerivadasRelacionadas) {
                derivadasFiles.push_back(file);
            } else {
                regularFiles.push_back(file);
            }
        }
        if (!regularFiles.empty()) {
            if (!importPort_) {
                return notImplemented("import external files");
            }
            auto regularRequest = request;
            regularRequest.files = std::move(regularFiles);
            auto regularResult = importPort_->importExternalFiles(regularRequest, stopToken);
            if (!regularResult.ok() || derivadasFiles.empty()) {
                return regularResult;
            }
            if (!derivadasPort_) {
                return notImplemented("derivadas import");
            }
            auto derivadasResult = derivadasPort_->importDerivations(
                {std::move(derivadasFiles), request.execution, request.progress}, stopToken);
            return mergeDerivadasOutcome(std::move(regularResult), derivadasResult);
        }
        if (!derivadasPort_) {
            return notImplemented("derivadas import");
        }
        return derivadasPort_->importDerivations(
            {std::move(derivadasFiles), request.execution, request.progress}, stopToken);
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

    ports::WorkflowResult
    SsaWorkflowService::importDerivations(const ports::ImportDerivationsRequest& request,
                                          std::stop_token stopToken) const {
        if (!derivadasPort_) {
            return notImplemented("derivadas import");
        }
        return derivadasPort_->importDerivations(request, std::move(stopToken));
    }

    bool SsaWorkflowService::legacySpreadsheetConverterAvailable() const {
        return derivadasPort_ && derivadasPort_->legacySpreadsheetConverterAvailable();
    }

    ports::WorkflowResult
    SsaWorkflowService::cleanOrphanDerivations(std::stop_token stopToken) const {
        if (!derivadasPort_) {
            return notImplemented("orphan derivation cleanup");
        }
        return derivadasPort_->cleanOrphanDerivations(std::move(stopToken));
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
        if (!samImportPort_) {
            return withCleanupStatus(notImplemented("SAM import"), samPort_->discardArtifacts());
        }
        auto result =
            samImportPort_->importSamArtifacts({.artifacts = std::move(fetchResult.artifacts),
                                                .rowsPerChunk = request.rowsPerChunk,
                                                .sqliteBusyWait = request.sqliteBusyWait},
                                               stopToken);
        return withCleanupStatus(std::move(result), samPort_->discardArtifacts());
    }

    ports::WorkflowResult SsaWorkflowService::notImplemented(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
