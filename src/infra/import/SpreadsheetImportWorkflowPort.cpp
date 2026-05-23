#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include <memory>
#include <sstream>
#include <utility>

namespace ssa::infra::importing {

    namespace {

        std::string workflowMessage(const char* operation, const ImportStagingResult& files,
                                    const SsaImportWriteSummary& writeSummary,
                                    const std::size_t failedFiles) {
            std::ostringstream output;
            output << operation << " files=" << files.xlsxFiles.size()
                   << " rows=" << writeSummary.rowsWritten
                   << " skipped=" << writeSummary.skippedRows
                   << " duplicates=" << writeSummary.duplicateRows
                   << " legacy_xls=" << files.legacyXls << " converted_xls=" << files.convertedXls
                   << " failed_legacy_xls=" << files.failedLegacyXls
                   << " unsupported=" << files.unsupported
                   << " failed=" << (failedFiles + files.failedCopies);
            return output.str();
        }

        std::string rejectedMessage(const char* operation, const ImportStagingResult& files) {
            SsaImportWriteSummary empty;
            return workflowMessage(operation, files, empty, 0);
        }

    } // namespace

    SpreadsheetImportWorkflowPort::SpreadsheetImportWorkflowPort(
        std::filesystem::path inputFolder, std::filesystem::path databasePath,
        std::vector<domain::ColumnDef> columns, LegacySpreadsheetConverter legacyConverter)
        : stager_(std::move(inputFolder), std::move(legacyConverter)),
          writer_(std::move(databasePath), std::move(columns)) {}

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importExternalFiles(
        const ports::ImportExternalFilesRequest& request) {
        if (request.files.empty()) {
            return {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"};
        }
        return importDiscoveredFiles(stager_.stageExternalFiles(request.files), false);
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::rescan(const ports::RescanRequest& request) {
        const bool replaceAll = request.mode == ports::RescanMode::Full;
        return importDiscoveredFiles(stager_.stageInputFiles(), replaceAll);
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::importDiscoveredFiles(const ImportStagingResult& files,
                                                         const bool replaceAll) const {
        constexpr const char* operation = "import_xlsx_to_sqlite";
        if (files.xlsxFiles.empty()) {
            if (replaceAll) {
                ResolvedSsaImportRows emptyRows;
                const auto summary = writer_.write(emptyRows, 0, 0, true);
                return {ports::WorkflowStatus::Succeeded,
                        workflowMessage(operation, files, summary, 0)};
            }
            return {ports::WorkflowStatus::Rejected, rejectedMessage(operation, files)};
        }

        SsaImportWriteSummary totalSummary;
        totalSummary.files = files.xlsxFiles.size();
        std::unique_ptr<sqlite::SqliteSsaImportWriter::WriteSession> writeSession;
        if (replaceAll) {
            try {
                writeSession = std::make_unique<sqlite::SqliteSsaImportWriter::WriteSession>(
                    writer_.startSession(true));
            } catch (const std::exception&) {
                return {ports::WorkflowStatus::Failed,
                        workflowMessage(operation, files, totalSummary, 1)};
            }
        }
        std::size_t failedFiles = 0;
        std::size_t emptyFiles = 0;
        for (const auto& file : files.xlsxFiles) {
            SsaImportBatch batch;
            try {
                batch = mapper_.map(reader_.readFirstSheet(file));
            } catch (const std::exception&) {
                ++failedFiles;
                return {ports::WorkflowStatus::Failed,
                        workflowMessage(operation, files, totalSummary, failedFiles)};
            }
            if (batch.rows.empty()) {
                ++emptyFiles;
                totalSummary.skippedRows += batch.skippedRows;
                continue;
            }
            const std::vector<SsaImportBatch> singleBatch{std::move(batch)};
            const auto resolved =
                conflictResolver_.resolveForDeleteInsertUpsertBySsaNumberKeepingUnkeyedRows(
                    singleBatch);
            try {
                if (!writeSession) {
                    writeSession = std::make_unique<sqlite::SqliteSsaImportWriter::WriteSession>(
                        writer_.startSession(replaceAll));
                }
                writeSession->write(resolved, 1, singleBatch.front().skippedRows);
            } catch (const std::exception&) {
                ++failedFiles;
                return {ports::WorkflowStatus::Failed,
                        workflowMessage(operation, files, totalSummary, failedFiles)};
            }
        }
        if (writeSession) {
            const auto writeSummary = writeSession->finish();
            totalSummary.rowsWritten = writeSummary.rowsWritten;
            totalSummary.skippedRows += writeSummary.skippedRows;
            totalSummary.duplicateRows = writeSummary.duplicateRows;
        }
        if (totalSummary.rowsWritten == 0) {
            return {ports::WorkflowStatus::Succeeded,
                    workflowMessage(operation, files, totalSummary, failedFiles)};
        }

        (void)emptyFiles;
        return {ports::WorkflowStatus::Succeeded,
                workflowMessage(operation, files, totalSummary, failedFiles)};
    }

} // namespace ssa::infra::importing
