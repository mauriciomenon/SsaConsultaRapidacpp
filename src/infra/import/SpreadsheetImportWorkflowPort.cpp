#include "infra/import/SpreadsheetImportWorkflowPort.h"

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
                   << " unsupported=" << files.unsupported
                   << " failed=" << (failure.failedFiles + files.failedCopies);
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
            return {ports::WorkflowStatus::Rejected, std::string{operation} + " canceled"};
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
                                                         const std::stop_token stopToken) const {
        constexpr const char* operation = "import_xlsx_to_sqlite";
        if (stopToken.stop_requested()) {
            return canceled(operation);
        }
        if (!files.rejectionReason.empty()) {
            return {ports::WorkflowStatus::Rejected,
                    std::string{operation} + " " + files.rejectionReason};
        }
        if (files.files.empty()) {
            if (replaceAll) {
                try {
                    const auto summary =
                        writer_.write(ResolvedSsaImportRows{}, 0, 0, true, stopToken);
                    return {ports::WorkflowStatus::Succeeded,
                            workflowMessage(operation, files, summary)};
                } catch (const std::exception& exc) {
                    return {ports::WorkflowStatus::Failed,
                            workflowMessage(operation, files, {}, {1, exc.what()})};
                }
            }
            return {ports::WorkflowStatus::Rejected, rejectedMessage(operation, files)};
        }

        SsaImportWriteSummary totalSummary;
        totalSummary.files = files.files.size();
        std::unique_ptr<sqlite::SqliteSsaImportWriter::WriteSession> writeSession;
        try {
            writeSession = std::make_unique<sqlite::SqliteSsaImportWriter::WriteSession>(
                writer_.startSession(replaceAll, stopToken));
        } catch (const std::exception& exc) {
            return {ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, totalSummary, {1, exc.what()})};
        }
        std::size_t failedFiles = 0;
        std::vector<PendingImportOutcome> pendingOutcomes;
        pendingOutcomes.reserve(files.files.size());
        for (const auto& file : files.files) {
            if (stopToken.stop_requested()) {
                return canceled(operation);
            }
            SsaImportBatch batch;
            try {
                batch = mapper_.map(reader_.readFirstSheet(file.workbookPath, stopToken));
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return canceled(operation);
                }
                ++failedFiles;
                return {
                    ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, totalSummary, {failedFiles, error.what()})};
            } catch (const std::exception& exc) {
                ++failedFiles;
                return {ports::WorkflowStatus::Failed,
                        workflowMessage(operation, files, totalSummary, {failedFiles, exc.what()})};
            }
            pendingOutcomes.push_back({&file, !batch.rows.empty()});
            if (batch.rows.empty()) {
                totalSummary.skippedRows += batch.skippedRows;
                continue;
            }
            const std::vector<SsaImportBatch> singleBatch{std::move(batch)};
            const auto resolved =
                conflictResolver_.resolveForDeleteInsertUpsertBySsaNumberKeepingUnkeyedRows(
                    singleBatch);
            try {
                writeSession->write(resolved, 1, singleBatch.front().skippedRows);
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return canceled(operation);
                }
                ++failedFiles;
                return {
                    ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, totalSummary, {failedFiles, error.what()})};
            } catch (const std::exception& exc) {
                ++failedFiles;
                return {ports::WorkflowStatus::Failed,
                        workflowMessage(operation, files, totalSummary, {failedFiles, exc.what()})};
            }
        }
        try {
            const auto writeSummary = writeSession->finish();
            totalSummary.rowsWritten = writeSummary.rowsWritten;
            totalSummary.skippedRows += writeSummary.skippedRows;
            totalSummary.duplicateRows = writeSummary.duplicateRows;
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return canceled(operation);
            }
            return {ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, totalSummary, {1, error.what()})};
        } catch (const std::exception& error) {
            return {ports::WorkflowStatus::Failed,
                    workflowMessage(operation, files, totalSummary, {1, error.what()})};
        }

        std::vector<ImportManifestEntry> manifest;
        manifest.reserve(pendingOutcomes.size());
        for (const auto& outcome : pendingOutcomes) {
            manifest.push_back({outcome.file->consolidationSources, outcome.hasValidRows});
        }
        const auto consolidation = stager_.consolidate(manifest, stopToken);
        failedFiles += consolidation.failed;
        return {ports::WorkflowStatus::Succeeded,
                workflowMessage(operation, files, totalSummary, {failedFiles, consolidation.error}),
                consolidation.failed > 0 || consolidation.canceled};
    }

} // namespace ssa::infra::importing
