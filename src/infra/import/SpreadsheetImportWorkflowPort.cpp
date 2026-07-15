#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QLockFile>

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

        constexpr std::size_t kMaxWorkflowDiagnosticBytes = 4'096;

        void appendWorkflowDiagnostic(std::string& destination, const std::string_view detail) {
            if (detail.empty() || destination.size() >= kMaxWorkflowDiagnosticBytes) {
                return;
            }
            if (!destination.empty()) {
                destination += "; ";
            }
            destination.append(detail.substr(
                0, (std::min)(detail.size(), kMaxWorkflowDiagnosticBytes - destination.size())));
        }

        std::unique_ptr<QLockFile> acquireImportLock(const std::filesystem::path& path,
                                                     QLockFile::LockError& error) {
            auto lock = std::make_unique<QLockFile>(qt::toQString(path));
            lock->setStaleLockTime(0);
            if (lock->tryLock(0)) {
                error = QLockFile::NoError;
                return lock;
            }
            error = lock->error();
            return {};
        }

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

        std::string_view importLockDiagnostic(const QLockFile::LockError error) {
            switch (error) {
            case QLockFile::NoError:
                return "QLockFile acquisition failed without an error code";
            case QLockFile::LockFailedError:
                return "QLockFile is held by another process";
            case QLockFile::PermissionError:
                return "QLockFile permission denied";
            case QLockFile::UnknownError:
                return "QLockFile reported an unknown filesystem error";
            }
            return "QLockFile reported an invalid error code";
        }

        ports::ImportSummary
        selectedFilesFailureSummary(const std::vector<std::filesystem::path>& files) {
            ports::ImportSummary summary;
            summary.discovered = files.size();
            summary.rejected = files.size();
            summary.preserved = files.size();
            summary.files.reserve(files.size());
            for (const auto& file : files) {
                summary.files.push_back({.source = qt::toUtf8(file.filename()),
                                         .status = ports::ImportFileStatus::Failed});
            }
            return summary;
        }

        ports::WorkflowResult importLockFailure(const QLockFile::LockError error,
                                                const ports::ImportSummary& summary = {}) {
            return withSummary({ports::WorkflowStatus::Failed,
                                error == QLockFile::LockFailedError ? "import_already_running"
                                                                    : "import_lock_failed",
                                false, std::string{importLockDiagnostic(error)}},
                               summary);
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
        : importLockPath_(std::filesystem::absolute(inputFolder).lexically_normal().parent_path() /
                          ".ssa_import.lock"),
          stager_(std::move(inputFolder)), writer_(std::move(databasePath), std::move(columns)) {}

    std::optional<ports::WorkflowResult> SpreadsheetImportWorkflowPort::resumePendingConsolidation(
        const std::stop_token& stopToken) const {
        std::vector<ImportConsolidationMove> pending;
        try {
            const auto lookupToken = stopToken.stop_requested() ? std::stop_token{} : stopToken;
            pending = writer_.pendingConsolidation(lookupToken);
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return ports::WorkflowResult{ports::WorkflowStatus::Canceled,
                                             "import_consolidation_resume canceled"};
            }
            return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                         "import_consolidation_resume_failed", false, error.what()};
        } catch (const ports::OperationError& error) {
            return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                         "import_consolidation_resume_failed", false,
                                         error.diagnostic()};
        } catch (const std::exception& error) {
            return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                         "import_consolidation_resume_failed", false, error.what()};
        }
        if (pending.empty()) {
            return std::nullopt;
        }

        ImportConsolidationPlan plan;
        plan.entries.reserve(pending.size());
        for (auto& move : pending) {
            plan.entries.push_back({{std::move(move)}});
        }
        const auto consolidation = stager_.consolidate(plan, stopToken);
        std::vector<std::filesystem::path> completedSources;
        std::size_t journalFailures = 0;
        auto diagnostic = consolidation.error;
        for (std::size_t entryIndex = 0; entryIndex < plan.entries.size(); ++entryIndex) {
            if (entryIndex >= consolidation.entries.size()) {
                break;
            }
            const auto& planEntry = plan.entries[entryIndex];
            const auto& resultEntry = consolidation.entries[entryIndex];
            for (std::size_t moveIndex = 0; moveIndex < planEntry.moves.size(); ++moveIndex) {
                if (moveIndex >= resultEntry.moves.size() ||
                    !resultEntry.moves[moveIndex].completed) {
                    continue;
                }
                completedSources.push_back(planEntry.moves[moveIndex].source);
            }
        }
        try {
            writer_.completeConsolidation(completedSources);
        } catch (const ports::OperationError& error) {
            journalFailures = completedSources.size();
            appendWorkflowDiagnostic(diagnostic, error.diagnostic());
        } catch (const std::exception& error) {
            journalFailures = completedSources.size();
            appendWorkflowDiagnostic(diagnostic, error.what());
        }
        const auto completed = journalFailures == 0 ? completedSources.size() : std::size_t{0};
        if (consolidation.canceled) {
            return ports::WorkflowResult{
                ports::WorkflowStatus::Succeeded,
                "import_consolidation_resume canceled completed=" + std::to_string(completed) +
                    " pending=" + std::to_string(pending.size() - completed),
                true, std::move(diagnostic)};
        }
        if (consolidation.failed > 0 || journalFailures > 0) {
            return ports::WorkflowResult{
                ports::WorkflowStatus::Failed,
                "import_consolidation_resume_failed completed=" + std::to_string(completed) +
                    " pending=" + std::to_string(pending.size() - completed),
                true, std::move(diagnostic)};
        }
        return ports::WorkflowResult{ports::WorkflowStatus::Succeeded,
                                     "import_consolidation_resumed files=" +
                                         std::to_string(completed)};
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importExternalFiles(
        const ports::ImportExternalFilesRequest& request, const std::stop_token stopToken) {
        if (request.files.empty()) {
            return withSummary(
                {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"}, {});
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        const auto importLock = acquireImportLock(importLockPath_, lockError);
        if (!importLock) {
            return importLockFailure(lockError, selectedFilesFailureSummary(request.files));
        }
        const auto staging = stager_.stageExternalFiles(request.files, stopToken);
        if (auto resumed = resumePendingConsolidation(stopToken)) {
            if (resumed->status == ports::WorkflowStatus::Canceled) {
                return importDiscoveredFiles(staging, false, stopToken);
            }
            const auto cleanupDiagnostic = stager_.discardOwnedArtifacts(staging);
            if (!cleanupDiagnostic.empty()) {
                resumed->status = ports::WorkflowStatus::Failed;
                resumed->message = "import_xlsx_to_sqlite staging_cleanup_failed";
                appendWorkflowDiagnostic(resumed->diagnostic, cleanupDiagnostic);
            }
            return std::move(*resumed);
        }
        return importDiscoveredFiles(staging, false, stopToken);
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::rescan(const ports::RescanRequest& request,
                                                                const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled("rescan");
        }
        const bool replaceAll = request.mode == ports::RescanMode::Full;
        const auto directoryStatus = stager_.validateInputDirectory(stopToken);
        if (!directoryStatus.rejectionReason.empty()) {
            return importDiscoveredFiles(directoryStatus, replaceAll, stopToken);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        const auto importLock = acquireImportLock(importLockPath_, lockError);
        if (!importLock) {
            return importLockFailure(lockError);
        }
        const auto staging = stager_.stageInputFiles(stopToken, replaceAll);
        if (auto resumed = resumePendingConsolidation(stopToken)) {
            if (resumed->status == ports::WorkflowStatus::Canceled) {
                return importDiscoveredFiles(staging, replaceAll, stopToken);
            }
            return std::move(*resumed);
        }
        return importDiscoveredFiles(staging, replaceAll, stopToken);
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
        std::vector<ImportManifestEntry> manifest;
        manifest.reserve(pendingOutcomes.size());
        for (const auto& outcome : pendingOutcomes) {
            manifest.push_back({outcome.file->consolidationSources, outcome.hasValidRows});
        }
        const auto consolidationPlan = stager_.planConsolidation(manifest, stopToken);
        if (consolidationPlan.canceled) {
            return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
        }
        if (!consolidationPlan.error.empty()) {
            return discardBeforeCommit(rollbackSession(
                *writeSession, failed(operation, files, totalSummary, 1, consolidationPlan.error)));
        }
        try {
            std::vector<ImportConsolidationMove> journalMoves;
            for (const auto& entry : consolidationPlan.entries) {
                journalMoves.insert(journalMoves.end(), entry.moves.begin(), entry.moves.end());
            }
            writeSession->recordConsolidation(journalMoves);
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
        const auto consolidation = stager_.consolidate(consolidationPlan, stopToken);
        auto diagnostic = files.diagnostic;
        appendWorkflowDiagnostic(diagnostic, consolidation.error);
        std::vector<std::filesystem::path> completedSources;
        std::size_t journalFailures = 0;
        for (std::size_t entryIndex = 0; entryIndex < consolidationPlan.entries.size();
             ++entryIndex) {
            if (entryIndex >= consolidation.entries.size()) {
                break;
            }
            const auto& planEntry = consolidationPlan.entries[entryIndex];
            const auto& resultEntry = consolidation.entries[entryIndex];
            for (std::size_t moveIndex = 0; moveIndex < planEntry.moves.size(); ++moveIndex) {
                if (moveIndex >= resultEntry.moves.size() ||
                    !resultEntry.moves[moveIndex].completed) {
                    continue;
                }
                completedSources.push_back(planEntry.moves[moveIndex].source);
            }
        }
        try {
            writer_.completeConsolidation(completedSources);
        } catch (const ports::OperationError& error) {
            journalFailures = completedSources.size();
            appendWorkflowDiagnostic(diagnostic, error.diagnostic());
        } catch (const std::exception& error) {
            journalFailures = completedSources.size();
            appendWorkflowDiagnostic(diagnostic, error.what());
        }
        failedFiles += consolidation.failed + journalFailures;
        const auto consolidationState = consolidation.failed > 0 || journalFailures > 0
                                            ? std::string_view{"consolidation_failed"}
                                        : consolidation.canceled
                                            ? std::string_view{"consolidation_canceled"}
                                            : std::string_view{};
        std::size_t completedFiles = 0;
        for (std::size_t index = 0; index < pendingOutcomes.size(); ++index) {
            const auto& outcome = pendingOutcomes[index];
            if (outcome.file->consolidationSources.empty() ||
                index >= consolidation.entries.size()) {
                continue;
            }
            const auto& entry = consolidation.entries[index];
            if (entry.failed > 0 || entry.completed != outcome.file->consolidationSources.size()) {
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
        const bool postCommitInterrupted =
            consolidation.failed > 0 || consolidation.canceled || journalFailures > 0;
        const auto status = postCommitInterrupted || totalSummary.rowsWritten > 0
                                ? ports::WorkflowStatus::Succeeded
                                : ports::WorkflowStatus::NoChanges;
        return withSummary(
            {status,
             workflowMessage(operation, files, totalSummary, {failedFiles, consolidationState}),
             files.warning || files.failedCopies > 0 || files.failedLegacyXls > 0 ||
                 consolidation.failed > 0 || consolidation.canceled || journalFailures > 0 ||
                 totalSummary.invalidRows > 0 || totalSummary.duplicateRows > 0,
             std::move(diagnostic)},
            importSummary);
    }

} // namespace ssa::infra::importing
