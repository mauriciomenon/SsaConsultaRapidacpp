#include "application/SsaWorkflowService.h"

#include "domain/SsaImportPolicy.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ssa::application {

    namespace {

        // path::string() uses the ANSI code page on Windows (garbles or throws); keep UTF-8.
        std::string utf8FileName(const std::filesystem::path& file) {
            const auto name = file.filename().u8string();
            return {reinterpret_cast<const char*>(name.data()), name.size()};
        }

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

        // The summary drives the user-facing text; a derivadas file missing from it would read
        // as if it had never been selected.
        void appendDerivadasFiles(ports::WorkflowResult& result,
                                  const std::vector<std::filesystem::path>& derivadasFiles,
                                  const ports::ImportFileStatus status, const std::string& reason) {
            if (!result.importSummary.has_value()) {
                return;
            }
            auto& summary = *result.importSummary;
            summary.discovered += derivadasFiles.size();
            if (status == ports::ImportFileStatus::Rejected) {
                summary.rejected += derivadasFiles.size();
            } else if (status == ports::ImportFileStatus::Failed) {
                summary.failed += derivadasFiles.size();
            }
            for (const auto& file : derivadasFiles) {
                summary.files.push_back(
                    {.source = utf8FileName(file), .status = status, .reason = reason});
            }
        }

        // Derivadas edges need the SSA rows, so they are not imported when that part did not
        // finish; they are still listed as not applied.
        ports::WorkflowResult
        withSkippedDerivadas(ports::WorkflowResult regularResult,
                             const std::vector<std::filesystem::path>& derivadasFiles) {
            const bool canceled = regularResult.status == ports::WorkflowStatus::Canceled;
            appendDerivadasFiles(regularResult, derivadasFiles,
                                 canceled ? ports::ImportFileStatus::Canceled
                                          : ports::ImportFileStatus::Rejected,
                                 canceled ? "canceled" : "derivadas_not_run");
            return regularResult;
        }

        ports::WorkflowResult
        mergeDerivadasOutcome(ports::WorkflowResult regularResult,
                              const ports::WorkflowResult& derivadasResult,
                              const std::vector<std::filesystem::path>& derivadasFiles) {
            if (derivadasResult.ok()) {
                // Unchanged SSA workbooks plus applied derivadas still changed the database.
                if (derivadasResult.status == ports::WorkflowStatus::Succeeded) {
                    regularResult.status = ports::WorkflowStatus::Succeeded;
                }
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
            // Without these entries a failed derivadas run would be reported as "no failure".
            const bool rejected = derivadasResult.status == ports::WorkflowStatus::Rejected;
            appendDerivadasFiles(regularResult, derivadasFiles,
                                 rejected ? ports::ImportFileStatus::Rejected
                                          : ports::ImportFileStatus::Failed,
                                 rejected ? "" : "operation_failed");
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
            if (domain::SsaImportPolicy::classifySourceProfile(utf8FileName(file)) ==
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
            if (!regularResult.ok()) {
                return withSkippedDerivadas(std::move(regularResult), derivadasFiles);
            }
            if (derivadasFiles.empty()) {
                return regularResult;
            }
            if (!derivadasPort_) {
                return notImplemented("derivadas import");
            }
            const ports::ImportDerivationsRequest derivadasRequest{
                std::move(derivadasFiles), request.execution, request.progress};
            const auto derivadasResult =
                derivadasPort_->importDerivations(derivadasRequest, stopToken);
            return mergeDerivadasOutcome(std::move(regularResult), derivadasResult,
                                         derivadasRequest.files);
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
