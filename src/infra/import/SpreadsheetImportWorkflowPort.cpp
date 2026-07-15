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
            std::size_t summaryIndex = 0;
        };

        ports::ImportSummary makeImportSummary(const ImportStagingResult& files) {
            ports::ImportSummary summary;
            summary.discovered = files.discovered;
            summary.pending = files.legacyXls + files.unsupported;
            summary.files.reserve(files.discoveredXlsxSources.size());
            for (const auto& source : files.discoveredXlsxSources) {
                summary.files.push_back({.source = source});
            }
            std::vector<bool> staged(summary.files.size(), false);
            for (const auto& file : files.files) {
                staged[file.summaryIndex] = true;
            }
            for (std::size_t index = 0; index < staged.size(); ++index) {
                if (staged[index]) {
                    continue;
                }
                summary.files[index].status = ports::ImportFileStatus::Failed;
                ++summary.rejected;
                ++summary.preserved;
            }
            return summary;
        }

        void markUncommitted(ports::ImportSummary& summary, const ports::ImportFileStatus status) {
            summary.accepted = 0;
            summary.rejected =
                status == ports::ImportFileStatus::Canceled ? 0 : summary.files.size();
            summary.preserved = summary.files.size();
            summary.inserts = 0;
            summary.updates = 0;
            summary.unchangedRows = 0;
            for (auto& file : summary.files) {
                file.status = status;
                file.inserts = 0;
                file.updates = 0;
                file.unchangedRows = 0;
                file.consolidated = false;
                file.noSurvivor = false;
            }
        }

        ports::WorkflowResult withSummary(ports::WorkflowResult result,
                                          const ports::ImportSummary& summary) {
            result.importSummary = summary;
            return result;
        }

        std::string workflowMessage(const char* operation, const ImportStagingResult& files,
                                    const SsaImportWriteSummary& writeSummary,
                                    const WorkflowFailure& failure = WorkflowFailure{}) {
            std::ostringstream output;
            output << operation << " files=" << files.files.size()
                   << " rows=" << writeSummary.rowsWritten
                   << " inserted=" << writeSummary.rowsInserted
                   << " updated=" << writeSummary.rowsUpdated
                   << " unchanged=" << writeSummary.rowsUnchanged
                   << " skipped=" << writeSummary.skippedRows
                   << " duplicates=" << writeSummary.duplicateRows
                   << " conflicts=" << writeSummary.conflictRows
                   << " invalid_rows=" << writeSummary.invalidRows
                   << " invalid_number=" << writeSummary.invalidNumberRows
                   << " invalid_description=" << writeSummary.invalidDescriptionRows
                   << " invalid_date=" << writeSummary.invalidDateRows
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
            return withSummary(
                {ports::WorkflowStatus::Canceled, std::string{operation} + " canceled"}, {});
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
        std::vector<domain::ColumnDef> columns)
        : stager_(std::move(inputFolder)), writer_(std::move(databasePath), std::move(columns)) {}

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importExternalFiles(
        const ports::ImportExternalFilesRequest& request, const std::stop_token stopToken) {
        if (request.files.empty()) {
            return withSummary(
                {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"}, {});
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
        auto importSummary = makeImportSummary(files);
        const auto discardBeforeCommit = [this, &files,
                                          &importSummary](ports::WorkflowResult result) {
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
            const auto fileStatus = result.status == ports::WorkflowStatus::Canceled
                                        ? ports::ImportFileStatus::Canceled
                                    : result.status == ports::WorkflowStatus::Rejected
                                        ? ports::ImportFileStatus::Rejected
                                        : ports::ImportFileStatus::Failed;
            markUncommitted(importSummary, fileStatus);
            return withSummary(std::move(result), importSummary);
        };
        if (files.rejectionReason == "staging_cleanup_failed") {
            markUncommitted(importSummary, ports::ImportFileStatus::Failed);
            return withSummary({ports::WorkflowStatus::Failed,
                                "import_xlsx_to_sqlite staging_cleanup_failed", false,
                                files.diagnostic},
                               importSummary);
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
                return withSummary({ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " no_importable_files"},
                                   importSummary);
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
        for (std::size_t fileIndex = 0; fileIndex < files.files.size(); ++fileIndex) {
            const auto& file = files.files[fileIndex];
            auto& fileResult = importSummary.files[file.summaryIndex];
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            SsaImportBatch batch;
            try {
                auto tables = reader_.readSheets(file.workbookPath, stopToken);
                bool mappedWorksheet = false;
                bool invalidTrailingWorksheet = false;
                batch.sourcePath = file.workbookPath;
                for (auto& table : tables) {
                    table.originalFilename = file.originalFilename;
                    table.sourceModifiedTimestamp = file.sourceModifiedTimestamp;
                    auto worksheetBatch = mapper_.map(table, stopToken);
                    if (worksheetBatch.mappingStatus ==
                        SpreadsheetMappingStatus::HeaderNotRecognized) {
                        if (mappedWorksheet) {
                            invalidTrailingWorksheet = true;
                            break;
                        }
                        batch.skippedRows += worksheetBatch.skippedRows;
                        continue;
                    }
                    if (worksheetBatch.mappingStatus != SpreadsheetMappingStatus::Mapped) {
                        batch.mappingStatus = worksheetBatch.mappingStatus;
                        break;
                    }
                    mappedWorksheet = true;
                    batch.mappedColumns += worksheetBatch.mappedColumns;
                    batch.skippedRows += worksheetBatch.skippedRows;
                    batch.invalidRows += worksheetBatch.invalidRows;
                    batch.invalidNumberRows += worksheetBatch.invalidNumberRows;
                    batch.invalidDescriptionRows += worksheetBatch.invalidDescriptionRows;
                    batch.invalidDateRows += worksheetBatch.invalidDateRows;
                    batch.rows.insert(batch.rows.end(),
                                      std::make_move_iterator(worksheetBatch.rows.begin()),
                                      std::make_move_iterator(worksheetBatch.rows.end()));
                }
                if (invalidTrailingWorksheet) {
                    batch.mappingStatus = SpreadsheetMappingStatus::HeaderNotRecognized;
                } else if (batch.mappingStatus !=
                               SpreadsheetMappingStatus::RequiredColumnsMissing &&
                           batch.mappingStatus != SpreadsheetMappingStatus::AmbiguousHeaders) {
                    batch.mappingStatus = mappedWorksheet
                                              ? SpreadsheetMappingStatus::Mapped
                                              : SpreadsheetMappingStatus::HeaderNotRecognized;
                }
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
            if (batch.mappingStatus == SpreadsheetMappingStatus::HeaderNotRecognized) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " header_not_recognized"}));
            }
            if (batch.mappingStatus == SpreadsheetMappingStatus::RequiredColumnsMissing) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " required_columns_missing"}));
            }
            if (batch.mappingStatus == SpreadsheetMappingStatus::AmbiguousHeaders) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " ambiguous_headers"}));
            }
            fileResult.validRows = batch.rows.size();
            fileResult.invalidRows = batch.invalidRows;
            importSummary.validRows += batch.rows.size();
            importSummary.invalidRows += batch.invalidRows;
            totalSummary.invalidRows += batch.invalidRows;
            totalSummary.invalidNumberRows += batch.invalidNumberRows;
            totalSummary.invalidDescriptionRows += batch.invalidDescriptionRows;
            totalSummary.invalidDateRows += batch.invalidDateRows;
            if (replaceAll && batch.invalidRows > 0) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     workflowMessage(operation, files, totalSummary, {0, "invalid_rows"})}));
            }
            pendingOutcomes.push_back({&file, !batch.rows.empty(), file.summaryIndex});
            if (batch.rows.empty()) {
                fileResult.status = ports::ImportFileStatus::NoValidRows;
                totalSummary.skippedRows += batch.skippedRows;
                if (replaceAll) {
                    return discardBeforeCommit(rollbackSession(
                        *writeSession, {ports::WorkflowStatus::Rejected,
                                        std::string{operation} + " no_valid_rows"}));
                }
                continue;
            }
            hasAnyValidRows = true;
            const std::vector<SsaImportBatch> singleBatch{std::move(batch)};
            const auto resolved =
                conflictResolver_.resolveBySsaNumberKeepingUnkeyedRows(singleBatch);
            if (resolved.conflictRows > 0) {
                fileResult.conflicts = resolved.conflictRows;
                importSummary.conflicts += resolved.conflictRows;
                totalSummary.conflictRows += resolved.conflictRows;
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     workflowMessage(operation, files, totalSummary, {0, "duplicate_conflict"})}));
            }
            try {
                const auto batchWrite =
                    writeSession->write(resolved, 1, singleBatch.front().skippedRows);
                fileResult.inserts = batchWrite.rowsInserted;
                fileResult.updates = batchWrite.rowsUpdated;
                fileResult.unchangedRows = batchWrite.rowsUnchanged;
                fileResult.conflicts = batchWrite.conflictRows;
                importSummary.inserts += batchWrite.rowsInserted;
                importSummary.updates += batchWrite.rowsUpdated;
                importSummary.unchangedRows += batchWrite.rowsUnchanged;
                importSummary.conflicts += batchWrite.conflictRows;
                fileResult.status = batchWrite.rowsWritten > 0 ? ports::ImportFileStatus::Applied
                                                               : ports::ImportFileStatus::NoChanges;
                if (batchWrite.conflictRows > 0) {
                    totalSummary.conflictRows += batchWrite.conflictRows;
                    return discardBeforeCommit(rollbackSession(
                        *writeSession, {ports::WorkflowStatus::Rejected,
                                        workflowMessage(operation, files, totalSummary,
                                                        {0, "duplicate_conflict"})}));
                }
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
            totalSummary.rowsInserted = writeSummary.rowsInserted;
            totalSummary.rowsUpdated = writeSummary.rowsUpdated;
            totalSummary.rowsUnchanged = writeSummary.rowsUnchanged;
            totalSummary.skippedRows += writeSummary.skippedRows;
            totalSummary.duplicateRows = writeSummary.duplicateRows;
            importSummary.inserts = writeSummary.rowsInserted;
            importSummary.updates = writeSummary.rowsUpdated;
            importSummary.unchangedRows = writeSummary.rowsUnchanged;
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

        importSummary.accepted = pendingOutcomes.size();

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
        std::size_t completedFiles = 0;
        for (std::size_t index = 0; index < pendingOutcomes.size(); ++index) {
            const auto& outcome = pendingOutcomes[index];
            if (outcome.file->consolidationSources.empty() ||
                index >= consolidation.entries.size()) {
                continue;
            }
            const auto& entry = consolidation.entries[index];
            if (entry.failed > 0 || entry.moved != outcome.file->consolidationSources.size()) {
                continue;
            }
            auto& fileResult = importSummary.files[outcome.summaryIndex];
            fileResult.consolidated = outcome.hasValidRows;
            fileResult.noSurvivor = !outcome.hasValidRows;
            importSummary.consolidated += outcome.hasValidRows ? 1 : 0;
            importSummary.noSurvivor += outcome.hasValidRows ? 0 : 1;
            ++completedFiles;
        }
        importSummary.preserved = importSummary.files.size() > completedFiles
                                      ? importSummary.files.size() - completedFiles
                                      : 0;
        const auto status = totalSummary.rowsWritten == 0 ? ports::WorkflowStatus::NoChanges
                                                          : ports::WorkflowStatus::Succeeded;
        return withSummary(
            {status,
             workflowMessage(operation, files, totalSummary, {failedFiles, consolidationState}),
             files.warning || files.failedCopies > 0 || files.failedLegacyXls > 0 ||
                 consolidation.failed > 0 || consolidation.canceled ||
                 totalSummary.invalidRows > 0 || totalSummary.duplicateRows > 0,
             std::move(diagnostic)},
            importSummary);
    }

} // namespace ssa::infra::importing
