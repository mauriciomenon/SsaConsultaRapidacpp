#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include "ports/OperationError.h"

#include <memory>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace ssa::infra::importing {

    namespace {

        struct WorkflowFailure {
            std::size_t failedFiles = 0;
            std::string_view error = {};
        };

        struct PendingImportOutcome {
            const StagedImportFile* file = nullptr;
            bool hasValidRows = false;
        };

        std::string workflowMessage(const char* operation, const ImportStagingResult& files,
                                    const SsaImportWriteSummary& writeSummary,
                                    const WorkflowFailure& failure = WorkflowFailure{}) {
            std::ostringstream output;
            output << operation << " files=" << files.files.size()
                   << " rows=" << writeSummary.rowsWritten
                   << " skipped=" << writeSummary.skippedRows
                   << " duplicates=" << writeSummary.duplicateRows
                   << " legacy_xls=" << files.legacyXls << " converted_xls=" << files.convertedXls
                   << " failed_legacy_xls=" << files.failedLegacyXls
                   << " unsupported=" << files.unsupported << " failed="
                   << (failure.failedFiles + files.failedCopies + files.failedLegacyXls);
            if (!failure.error.empty()) {
                output << " error=" << failure.error;
            }
            return output.str();
        }

        std::string rejectedMessage(const char* operation, const ImportStagingResult& files) {
            SsaImportWriteSummary empty;
            return workflowMessage(operation, files, empty);
        }

        ports::WorkflowResult canceled(const char* operation) {
            return {ports::WorkflowStatus::Canceled, std::string{operation} + " canceled"};
        }

        ports::WorkflowResult failed(const char* operation, const ImportStagingResult& files,
                                     const SsaImportWriteSummary& summary,
                                     const std::size_t failedFiles, std::string diagnostic) {
            return {ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, summary, {failedFiles, "operation_failed"}),
                    false, std::move(diagnostic)};
        }

        ports::WorkflowResult rollbackSession(sqlite::SqliteSsaImportWriter::WriteSession& session,
                                              ports::WorkflowResult result) {
            try {
                session.rollback();
            } catch (const ports::OperationError& error) {
                result.status = ports::WorkflowStatus::Failed;
                result.message = "import_xlsx_to_sqlite rollback_failed";
                result.diagnostic += "; " + error.diagnostic();
            } catch (const std::exception& error) {
                result.status = ports::WorkflowStatus::Failed;
                result.message = "import_xlsx_to_sqlite rollback_failed";
                result.diagnostic += "; " + std::string{error.what()};
            }
            return result;
        }

    } // namespace

    SpreadsheetImportWorkflowPort::SpreadsheetImportWorkflowPort(
        std::filesystem::path inputFolder, std::filesystem::path databasePath,
        std::vector<domain::ColumnDef> columns, LegacySpreadsheetConverter legacyConverter)
        : stager_(std::move(inputFolder), std::move(legacyConverter)),
          writer_(std::move(databasePath), std::move(columns)) {}

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importExternalFiles(
        const ports::ImportExternalFilesRequest& request, const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled("import_external_files");
        }
        if (request.files.empty()) {
            return {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"};
        }
        return importDiscoveredFiles(stager_.stageExternalFiles(request.files, stopToken), false,
                                     stopToken);
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::rescan(const ports::RescanRequest& request,
                                                                const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled("rescan");
        }
        const bool replaceAll = request.mode == ports::RescanMode::Full;
        return importDiscoveredFiles(stager_.stageInputFiles(stopToken, replaceAll), replaceAll,
                                     stopToken);
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::importDiscoveredFiles(const ImportStagingResult& files,
                                                         const bool replaceAll,
                                                         const std::stop_token& stopToken) const {
        constexpr const char* operation = "import_xlsx_to_sqlite";
        const auto discardBeforeCommit = [this, &files](ports::WorkflowResult result) {
            if (!files.diagnostic.empty()) {
                if (!result.diagnostic.empty()) {
                    result.diagnostic += "; ";
                }
                result.diagnostic += files.diagnostic;
            }
            const auto cleanupDiagnostic = stager_.discardOwnedArtifacts(files);
            if (!cleanupDiagnostic.empty()) {
                if (!result.diagnostic.empty()) {
                    result.diagnostic += "; ";
                }
                result.diagnostic += cleanupDiagnostic;
            }
            if ((files.warning || !cleanupDiagnostic.empty()) &&
                result.status != ports::WorkflowStatus::Failed) {
                result.status = ports::WorkflowStatus::Failed;
                result.message = "import_xlsx_to_sqlite staging_cleanup_failed";
            }
            return result;
        };
        if (files.rejectionReason == "staging_cleanup_failed") {
            return {ports::WorkflowStatus::Failed, "import_xlsx_to_sqlite staging_cleanup_failed",
                    false, files.diagnostic};
        }
        if (files.operationalFailure) {
            return discardBeforeCommit({ports::WorkflowStatus::Failed,
                                        std::string{operation} + " " + files.rejectionReason});
        }
        if (stopToken.stop_requested()) {
            return discardBeforeCommit(canceled(operation));
        }
        if (!files.rejectionReason.empty()) {
            return discardBeforeCommit({ports::WorkflowStatus::Rejected,
                                        std::string{operation} + " " + files.rejectionReason});
        }
        if (replaceAll && (files.failedCopies > 0 || files.failedLegacyXls > 0)) {
            return discardBeforeCommit(
                {ports::WorkflowStatus::Failed, rejectedMessage(operation, files)});
        }
        if (files.files.empty()) {
            if (files.failedCopies > 0 || files.failedLegacyXls > 0) {
                return discardBeforeCommit(
                    {ports::WorkflowStatus::Failed, rejectedMessage(operation, files)});
            }
            if (replaceAll) {
                return {ports::WorkflowStatus::Rejected,
                        std::string{operation} + " no_importable_files"};
            }
            return discardBeforeCommit(
                {ports::WorkflowStatus::Rejected, rejectedMessage(operation, files)});
        }

        SsaImportWriteSummary totalSummary;
        totalSummary.files = files.files.size();
        std::unique_ptr<sqlite::SqliteSsaImportWriter::WriteSession> writeSession;
        try {
            writeSession = std::make_unique<sqlite::SqliteSsaImportWriter::WriteSession>(
                writer_.startSession(replaceAll, stopToken));
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return discardBeforeCommit(canceled(operation));
            }
            return discardBeforeCommit(failed(operation, files, totalSummary, 1, error.what()));
        } catch (const ports::OperationError& error) {
            return discardBeforeCommit(
                failed(operation, files, totalSummary, 1, error.diagnostic()));
        } catch (const std::exception& exc) {
            return discardBeforeCommit(failed(operation, files, totalSummary, 1, exc.what()));
        }
        std::size_t failedFiles = 0;
        bool hasAnyValidRows = false;
        std::vector<PendingImportOutcome> pendingOutcomes;
        pendingOutcomes.reserve(files.files.size());
        for (const auto& file : files.files) {
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            SsaImportBatch batch;
            try {
                batch = mapper_.map(reader_.readFirstSheet(file.workbookPath, stopToken));
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
                }
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, error.what())));
            } catch (const ports::OperationError& error) {
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, error.diagnostic())));
            } catch (const std::exception& exc) {
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, exc.what())));
            }
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            if (replaceAll &&
                batch.mappingStatus == SpreadsheetMappingStatus::HeaderNotRecognized) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " header_not_recognized"}));
            }
            pendingOutcomes.push_back({&file, !batch.rows.empty()});
            if (batch.rows.empty()) {
                totalSummary.skippedRows += batch.skippedRows;
                continue;
            }
            hasAnyValidRows = true;
            const std::vector<SsaImportBatch> singleBatch{std::move(batch)};
            const auto resolved =
                conflictResolver_.resolveForDeleteInsertUpsertBySsaNumberKeepingUnkeyedRows(
                    singleBatch);
            try {
                writeSession->write(resolved, 1, singleBatch.front().skippedRows);
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
                }
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, error.what())));
            } catch (const ports::OperationError& error) {
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, error.diagnostic())));
            } catch (const std::exception& exc) {
                ++failedFiles;
                return discardBeforeCommit(
                    rollbackSession(*writeSession, failed(operation, files, totalSummary,
                                                          failedFiles, exc.what())));
            }
        }
        if (replaceAll && !hasAnyValidRows) {
            return discardBeforeCommit(
                rollbackSession(*writeSession, {ports::WorkflowStatus::Rejected,
                                                std::string{operation} + " no_valid_rows"}));
        }
        try {
            const auto writeSummary = writeSession->finish();
            totalSummary.rowsWritten = writeSummary.rowsWritten;
            totalSummary.skippedRows += writeSummary.skippedRows;
            totalSummary.duplicateRows = writeSummary.duplicateRows;
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            return discardBeforeCommit(rollbackSession(
                *writeSession, failed(operation, files, totalSummary, 1, error.what())));
        } catch (const ports::OperationError& error) {
            return discardBeforeCommit(rollbackSession(
                *writeSession, failed(operation, files, totalSummary, 1, error.diagnostic())));
        } catch (const std::exception& error) {
            return discardBeforeCommit(rollbackSession(
                *writeSession, failed(operation, files, totalSummary, 1, error.what())));
        }

        std::vector<ImportManifestEntry> manifest;
        manifest.reserve(pendingOutcomes.size());
        for (const auto& outcome : pendingOutcomes) {
            manifest.push_back({outcome.file->consolidationSources, outcome.hasValidRows});
        }
        const auto consolidation = stager_.consolidate(manifest, stopToken);
        failedFiles += consolidation.failed;
        auto diagnostic = files.diagnostic;
        if (!consolidation.error.empty()) {
            if (!diagnostic.empty()) {
                diagnostic += "; ";
            }
            diagnostic += consolidation.error;
        }
        constexpr std::size_t kMaxDiagnosticBytes = 4'096;
        if (diagnostic.size() > kMaxDiagnosticBytes) {
            diagnostic.resize(kMaxDiagnosticBytes);
        }
        const auto consolidationState =
            consolidation.failed > 0 ? std::string_view{"consolidation_failed"}
            : consolidation.canceled ? std::string_view{"consolidation_canceled"}
                                     : std::string_view{};
        return {ports::WorkflowStatus::Succeeded,
                workflowMessage(operation, files, totalSummary, {failedFiles, consolidationState}),
                files.warning || files.failedCopies > 0 || files.failedLegacyXls > 0 ||
                    consolidation.failed > 0 || consolidation.canceled,
                std::move(diagnostic)};
    }

} // namespace ssa::infra::importing
