#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include "domain/SsaTypes.h"
#include "infra/import/SsaSpreadsheetMapper.h"
#include "infra/import/XlsxWorkbookReader.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QDate>
#include <QDebug>
#include <QLockFile>
#include <QTemporaryDir>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
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
        constexpr std::size_t kMaxImportFilesPerBatch = 64;
        constexpr int kDatabaseBackupPagesPerStep = 256;
        constexpr std::size_t kFileComparisonBlockBytes = std::size_t{64} * 1024;

        enum class FileContentsComparison {
            Same,
            Different,
            Canceled,
            Failed,
        };

        enum class DatabaseSnapshotPhase {
            InitialCopy,
            Publication,
        };

        [[nodiscard]] bool isRecoveryCopyCandidate(const StagedImportFile& selected,
                                                   const ImportConsolidationMove& pending) {
            const auto sourceName = qt::toQString(pending.source.filename());
            if (sourceName == qt::toQString(selected.consolidationFilename)) {
                return true;
            }
            const auto stagedPrefix = QStringLiteral(".ssa-staged-") +
                                      qt::toQString(selected.consolidationFilename.stem()) +
                                      QStringLiteral("_");
            return sourceName.startsWith(stagedPrefix) &&
                   pending.source.extension() == selected.consolidationFilename.extension();
        }

        [[nodiscard]] FileContentsComparison
        compareFileContents(const std::filesystem::path& first, const std::filesystem::path& second,
                            const std::stop_token& stopToken, std::string& diagnostic) {
            std::error_code error;
            const auto firstSize = std::filesystem::file_size(first, error);
            if (error) {
                diagnostic = "cannot inspect staged recovery candidate";
                return FileContentsComparison::Failed;
            }
            const auto secondSize = std::filesystem::file_size(second, error);
            if (error) {
                diagnostic = "cannot inspect pending recovery source";
                return FileContentsComparison::Failed;
            }
            if (firstSize != secondSize) {
                return FileContentsComparison::Different;
            }
            std::ifstream firstInput(first, std::ios::binary);
            std::ifstream secondInput(second, std::ios::binary);
            if (!firstInput || !secondInput) {
                diagnostic = "cannot open recovery duplicate candidate";
                return FileContentsComparison::Failed;
            }
            std::array<char, kFileComparisonBlockBytes> firstBuffer{};
            std::array<char, kFileComparisonBlockBytes> secondBuffer{};
            for (;;) {
                if (stopToken.stop_requested()) {
                    return FileContentsComparison::Canceled;
                }
                firstInput.read(firstBuffer.data(),
                                static_cast<std::streamsize>(firstBuffer.size()));
                secondInput.read(secondBuffer.data(),
                                 static_cast<std::streamsize>(secondBuffer.size()));
                const auto firstBytes = firstInput.gcount();
                const auto secondBytes = secondInput.gcount();
                if (firstBytes != secondBytes) {
                    return FileContentsComparison::Different;
                }
                if (firstBytes == 0) {
                    break;
                }
                const auto comparedBytes = static_cast<std::size_t>(firstBytes);
                if (!std::equal(firstBuffer.begin(), firstBuffer.begin() + comparedBytes,
                                secondBuffer.begin())) {
                    return FileContentsComparison::Different;
                }
            }
            if (!firstInput.eof() || !secondInput.eof()) {
                diagnostic = "cannot read recovery duplicate candidate";
                return FileContentsComparison::Failed;
            }
            return FileContentsComparison::Same;
        }

        [[nodiscard]] FileContentsComparison
        compareSelectedStaging(const StagedImportFile& selected,
                               const ImportConsolidationMove& pending,
                               const std::stop_token& stopToken, std::string& diagnostic) {
            std::error_code error;
            if (std::filesystem::equivalent(selected.workbookPath, pending.source, error) &&
                !error) {
                return FileContentsComparison::Same;
            }
            if (!isRecoveryCopyCandidate(selected, pending)) {
                return FileContentsComparison::Different;
            }
            return compareFileContents(selected.workbookPath, pending.source, stopToken,
                                       diagnostic);
        }

        void removeStagedSummaryIndices(ImportStagingResult& staging,
                                        const std::vector<std::size_t>& discardedIndices) {
            std::vector<bool> discarded(staging.discoveredXlsxSources.size(), false);
            for (const auto index : discardedIndices) {
                discarded[index] = true;
            }
            std::vector<std::size_t> remappedIndices(discarded.size());
            std::vector<std::string> remainingSources;
            remainingSources.reserve(staging.discoveredXlsxSources.size());
            std::size_t discardedCount = 0;
            for (std::size_t index = 0; index < staging.discoveredXlsxSources.size(); ++index) {
                if (discarded[index]) {
                    ++discardedCount;
                    continue;
                }
                remappedIndices[index] = remainingSources.size();
                remainingSources.push_back(std::move(staging.discoveredXlsxSources[index]));
            }
            std::erase_if(staging.files, [&discarded](const StagedImportFile& file) {
                return discarded[file.summaryIndex];
            });
            for (auto& file : staging.files) {
                file.summaryIndex = remappedIndices[file.summaryIndex];
            }
            staging.discovered -= discardedCount;
            staging.discoveredXlsxSources = std::move(remainingSources);
        }

        [[nodiscard]] SamSpreadsheetAdaptResult
        readAndAdaptSamWorkbook(const StagedImportFile& file, const ports::SamArtifact& artifact,
                                const std::stop_token& stopToken) {
            auto sheets = XlsxWorkbookReader::readSheets(file.workbookPath, stopToken);
            for (auto& table : sheets) {
                table.originalFilename = file.originalFilename;
                table.sourceModifiedTimestamp = file.sourceModifiedTimestamp;
                table.sourceCreatedTimestamp = file.sourceCreatedTimestamp;
            }
            return SamSpreadsheetAdapter::adapt(sheets, artifact, stopToken);
        }

        struct ChunkedWorkbookImportResult {
            SsaImportBatch batch;
            SsaImportBatchWriteSummary writeSummary;
            std::size_t validRows{0};
            std::size_t conflictRows{0};
            bool invalidFullBatch{false};
            bool duplicateConflict{false};
        };

        void readAndImportChunkedWorkbook(const StagedImportFile& file, const bool replaceAll,
                                          const SsaImportConflictResolver& conflictResolver,
                                          sqlite::SqliteSsaImportWriter::WriteSession& writeSession,
                                          const std::size_t rowsPerChunk,
                                          const std::stop_token& stopToken,
                                          ChunkedWorkbookImportResult& result) {
            result = {};
            bool mappedWorksheet = false;
            bool ignoredWorksheet = false;
            bool invalidTrailingWorksheet = false;
            bool fatalMapping = false;
            bool fileCountedByWriter = false;
            std::vector<std::string> headerRow;
            result.batch.sourcePath = file.workbookPath;
            XlsxWorkbookReader::readSheetChunks(
                file.workbookPath, rowsPerChunk,
                [&](SpreadsheetTable table, const bool firstInSheet, const bool) {
                    if (fatalMapping || invalidTrailingWorksheet || result.invalidFullBatch ||
                        result.duplicateConflict) {
                        return;
                    }
                    if (firstInSheet) {
                        ignoredWorksheet = false;
                        headerRow.clear();
                    } else if (ignoredWorksheet) {
                        result.batch.skippedRows += table.rows.size();
                        return;
                    } else {
                        table.headerRow = headerRow;
                    }
                    table.originalFilename = file.originalFilename;
                    table.sourceModifiedTimestamp = file.sourceModifiedTimestamp;
                    table.sourceCreatedTimestamp = file.sourceCreatedTimestamp;
                    auto worksheetBatch = SsaSpreadsheetMapper::map(table, stopToken);
                    if (worksheetBatch.mappingStatus ==
                        SpreadsheetMappingStatus::HeaderNotRecognized) {
                        if (mappedWorksheet) {
                            invalidTrailingWorksheet = true;
                        } else {
                            ignoredWorksheet = true;
                            result.batch.skippedRows += worksheetBatch.skippedRows;
                        }
                        return;
                    }
                    if (worksheetBatch.mappingStatus != SpreadsheetMappingStatus::Mapped) {
                        result.batch.mappingStatus = worksheetBatch.mappingStatus;
                        fatalMapping = true;
                        return;
                    }
                    if (firstInSheet) {
                        headerRow = worksheetBatch.headerRow;
                        result.batch.mappedColumns += worksheetBatch.mappedColumns;
                    }
                    mappedWorksheet = true;
                    result.batch.skippedRows += worksheetBatch.skippedRows;
                    result.batch.invalidRows += worksheetBatch.invalidRows;
                    result.batch.invalidNumberRows += worksheetBatch.invalidNumberRows;
                    result.batch.invalidDescriptionRows += worksheetBatch.invalidDescriptionRows;
                    result.batch.invalidDateRows += worksheetBatch.invalidDateRows;
                    result.validRows += worksheetBatch.rows.size();
                    if (replaceAll && worksheetBatch.invalidRows > 0) {
                        result.invalidFullBatch = true;
                        return;
                    }
                    if (worksheetBatch.rows.empty()) {
                        return;
                    }
                    const std::vector<SsaImportBatch> chunk{std::move(worksheetBatch)};
                    const auto resolved =
                        conflictResolver.resolveBySsaNumberKeepingUnkeyedRows(chunk);
                    result.conflictRows += resolved.conflictRows;
                    if (resolved.conflictRows > 0) {
                        result.duplicateConflict = true;
                        return;
                    }
                    const auto batchWrite =
                        writeSession.write(resolved, fileCountedByWriter ? 0 : 1, 0);
                    fileCountedByWriter = true;
                    result.writeSummary.rowsWritten += batchWrite.rowsWritten;
                    result.writeSummary.rowsInserted += batchWrite.rowsInserted;
                    result.writeSummary.rowsUpdated += batchWrite.rowsUpdated;
                    result.writeSummary.rowsUnchanged += batchWrite.rowsUnchanged;
                    result.writeSummary.duplicateRows += batchWrite.duplicateRows;
                    result.writeSummary.conflictRows += batchWrite.conflictRows;
                    result.duplicateConflict = batchWrite.conflictRows > 0;
                },
                stopToken);
            if (invalidTrailingWorksheet) {
                result.batch.mappingStatus = SpreadsheetMappingStatus::HeaderNotRecognized;
            } else if (!fatalMapping) {
                result.batch.mappingStatus = mappedWorksheet
                                                 ? SpreadsheetMappingStatus::Mapped
                                                 : SpreadsheetMappingStatus::HeaderNotRecognized;
            }
        }

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

        std::string sqliteBackupError(sqlite3* database, const char* operation, const int rc) {
            return std::string{operation} + " failed: rc=" + std::to_string(rc) +
                   " message=" + sqlite3_errmsg(database);
        }

        // Both call sites name source and destination explicitly.
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void backupDatabaseSnapshot(const std::filesystem::path& source,
                                    const std::filesystem::path& destination,
                                    const std::stop_token& stopToken,
                                    const std::chrono::milliseconds busyWait,
                                    const DatabaseSnapshotPhase phase,
                                    sqlite::SqliteSynchronizationSemaphore* snapshotLocked) {
            const bool publication = phase == DatabaseSnapshotPhase::Publication;
            const char* safeMessage = publication ? "Falha ao publicar banco do rescan"
                                                  : "Falha ao copiar banco para rescan";
            const char* operation =
                publication ? "sqlite backup publication" : "sqlite backup copy";
            if (!std::filesystem::exists(source)) {
                if (!publication) {
                    return;
                }
                throw ports::OperationError(safeMessage, std::string{operation} +
                                                             " failed: source database missing");
            }
            sqlite::SqliteConnection sourceConnection(source, sqlite::SqliteOpenMode::ReadOnly,
                                                      std::chrono::milliseconds{0});
            sqlite::SqliteConnection destinationConnection(
                destination, sqlite::SqliteOpenMode::ReadWriteCreate, std::chrono::milliseconds{0});
            std::atomic_flag snapshotLockReported = ATOMIC_FLAG_INIT;
            sqlite::SqliteBusyHandler busyHandler(destinationConnection.handle(), stopToken,
                                                  busyWait, publication ? snapshotLocked : nullptr,
                                                  publication ? &snapshotLockReported : nullptr);
            auto* backup = sqlite3_backup_init(destinationConnection.handle(), "main",
                                               sourceConnection.handle(), "main");
            if (backup == nullptr) {
                throw ports::OperationError(
                    safeMessage,
                    sqliteBackupError(destinationConnection.handle(),
                                      publication ? "sqlite backup publication init"
                                                  : "sqlite backup copy init",
                                      sqlite3_errcode(destinationConnection.handle())));
            }
            int stepResult = SQLITE_OK;
            auto lockedRetryDeadline = std::chrono::steady_clock::time_point{};
            while (stepResult == SQLITE_OK || stepResult == SQLITE_LOCKED) {
                if (stopToken.stop_requested()) {
                    stepResult = SQLITE_INTERRUPT;
                    break;
                }
                stepResult = sqlite3_backup_step(backup, kDatabaseBackupPagesPerStep);
                if (stepResult == SQLITE_LOCKED) {
                    if (publication && snapshotLocked != nullptr &&
                        !snapshotLockReported.test_and_set(std::memory_order_relaxed)) {
                        snapshotLocked->release();
                    }
                    const auto now = std::chrono::steady_clock::now();
                    if (lockedRetryDeadline == std::chrono::steady_clock::time_point{}) {
                        lockedRetryDeadline = now + busyWait;
                    }
                    if (now >= lockedRetryDeadline) {
                        break;
                    }
                    std::this_thread::sleep_until(
                        (std::min)(lockedRetryDeadline,
                                   now +
                                       ports::ImportExecutionOptions::kSqliteBusyRetryGranularity));
                } else {
                    lockedRetryDeadline = {};
                }
            }
            const int finishResult = sqlite3_backup_finish(backup);
            if (stepResult != SQLITE_DONE &&
                (stopToken.stop_requested() || stepResult == SQLITE_INTERRUPT)) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "database snapshot canceled");
            }
            if (stepResult != SQLITE_DONE || finishResult != SQLITE_OK) {
                const int errorCode = stepResult != SQLITE_DONE ? stepResult : finishResult;
                throw ports::OperationError(
                    safeMessage,
                    sqliteBackupError(destinationConnection.handle(), operation, errorCode));
            }
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

        std::optional<std::filesystem::path>
        resolvedImportFolder(const std::filesystem::path& inputFolder, std::string& diagnostic) {
            std::error_code error;
            auto normalized = std::filesystem::weakly_canonical(inputFolder, error);
            if (error) {
                const auto canonicalError = error.message();
                error.clear();
                normalized = std::filesystem::absolute(inputFolder, error);
                if (error) {
                    diagnostic = "cannot resolve canonical import corpus: weakly_canonical=" +
                                 canonicalError + "; absolute=" + error.message();
                    return std::nullopt;
                }
                normalized = normalized.lexically_normal();
            }
            return normalized;
        }

        struct ImportLocks final {
            std::unique_ptr<QLockFile> corpus;
            std::unique_ptr<sqlite::SqliteDatabaseWriteLock> database;

            explicit operator bool() const noexcept {
                return corpus != nullptr && database != nullptr;
            }
        };

        enum class ImportLockFailureOrigin { Corpus, Database };

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

        ImportLocks acquireImportLocks(const std::filesystem::path& corpusPath,
                                       const std::filesystem::path& databasePath,
                                       QLockFile::LockError& error, std::string& diagnostic,
                                       ImportLockFailureOrigin& failureOrigin) {
            failureOrigin = ImportLockFailureOrigin::Corpus;
            auto corpus = acquireImportLock(corpusPath, error);
            if (!corpus) {
                diagnostic = importLockDiagnostic(error);
                return {};
            }
            auto database = std::make_unique<sqlite::SqliteDatabaseWriteLock>(databasePath);
            if (!database->acquired()) {
                failureOrigin = ImportLockFailureOrigin::Database;
                error = database->error();
                diagnostic = database->diagnostic();
                return {};
            }
            error = QLockFile::NoError;
            diagnostic.clear();
            return {std::move(corpus), std::move(database)};
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
                if (files.operationalFailure) {
                    summary.files[index].status = ports::ImportFileStatus::Failed;
                    ++summary.failed;
                } else {
                    summary.files[index].status = ports::ImportFileStatus::Rejected;
                    ++summary.rejected;
                }
                ++summary.preserved;
            }
            return summary;
        }

        void markUncommitted(ports::ImportSummary& summary, const ports::ImportFileStatus status) {
            summary.accepted = 0;
            summary.rejected = 0;
            summary.ignored = 0;
            summary.failed = 0;
            summary.preserved = summary.files.size();
            summary.inserts = 0;
            summary.updates = 0;
            summary.unchangedRows = 0;
            for (auto& file : summary.files) {
                if (file.status != ports::ImportFileStatus::Ignored &&
                    file.status != ports::ImportFileStatus::Failed) {
                    file.status = status;
                }
                switch (file.status) {
                case ports::ImportFileStatus::Ignored:
                    ++summary.ignored;
                    break;
                case ports::ImportFileStatus::Rejected:
                    ++summary.rejected;
                    break;
                case ports::ImportFileStatus::Failed:
                    ++summary.failed;
                    break;
                default:
                    break;
                }
                if (file.reason.empty()) {
                    switch (file.status) {
                    case ports::ImportFileStatus::Rejected:
                        file.reason = "batch_rejected";
                        break;
                    case ports::ImportFileStatus::Failed:
                        file.reason = "operation_failed";
                        break;
                    case ports::ImportFileStatus::Canceled:
                        file.reason = "canceled";
                        break;
                    default:
                        break;
                    }
                }
                file.inserts = 0;
                file.updates = 0;
                file.unchangedRows = 0;
                file.consolidated = false;
                file.noSurvivor = false;
            }
        }

        void appendImportClassification(std::string& message, const ports::ImportSummary& summary) {
            message += " ignored=" + std::to_string(summary.ignored) +
                       " rejected=" + std::to_string(summary.rejected) +
                       " failed_files=" + std::to_string(summary.failed);
        }

        ports::WorkflowResult withSummary(ports::WorkflowResult result,
                                          const ports::ImportSummary& summary) {
            appendImportClassification(result.message, summary);
            result.importSummary = summary;
            return result;
        }

        ports::ImportSummary selectedFilesSummary(const std::vector<std::filesystem::path>& files,
                                                  const ports::ImportFileStatus status) {
            ports::ImportSummary summary;
            summary.discovered = files.size();
            summary.rejected = status == ports::ImportFileStatus::Rejected ? files.size() : 0;
            summary.failed = status == ports::ImportFileStatus::Failed ? files.size() : 0;
            summary.preserved = files.size();
            summary.files.reserve(files.size());
            for (const auto& file : files) {
                summary.files.push_back({.source = qt::toUtf8(file.filename()), .status = status});
            }
            return summary;
        }

        ports::WorkflowResult
        importLockFailure(const QLockFile::LockError error,
                          const ports::ImportSummary& summary = {},
                          const std::string_view diagnostic = {},
                          const ImportLockFailureOrigin origin = ImportLockFailureOrigin::Corpus) {
            const auto message = origin == ImportLockFailureOrigin::Database
                                     ? "database_write_lock_failed"
                                 : error == QLockFile::LockFailedError ? "import_already_running"
                                                                       : "import_lock_failed";
            return withSummary({ports::WorkflowStatus::Failed, message, false,
                                diagnostic.empty() ? std::string{importLockDiagnostic(error)}
                                                   : std::string{diagnostic}},
                               summary);
        }

        void mergeImportSummary(ports::ImportSummary& total, const ports::ImportSummary& batch) {
            total.discovered += batch.discovered;
            total.accepted += batch.accepted;
            total.rejected += batch.rejected;
            total.ignored += batch.ignored;
            total.failed += batch.failed;
            total.pending += batch.pending;
            total.preserved += batch.preserved;
            total.validRows += batch.validRows;
            total.invalidRows += batch.invalidRows;
            total.invalidNumberRows += batch.invalidNumberRows;
            total.invalidDescriptionRows += batch.invalidDescriptionRows;
            total.invalidDateRows += batch.invalidDateRows;
            total.skippedRows += batch.skippedRows;
            total.duplicateRows += batch.duplicateRows;
            total.inserts += batch.inserts;
            total.updates += batch.updates;
            total.unchangedRows += batch.unchangedRows;
            total.conflicts += batch.conflicts;
            total.consolidated += batch.consolidated;
            total.noSurvivor += batch.noSurvivor;
            total.files.insert(total.files.end(), batch.files.begin(), batch.files.end());
        }

        std::string batchedImportMessage(const ports::ImportSummary& summary,
                                         const std::size_t batches) {
            std::ostringstream output;
            output << "import_xlsx_to_sqlite batches=" << batches
                   << " files=" << summary.files.size() << " rows=" << summary.validRows
                   << " inserted=" << summary.inserts << " updated=" << summary.updates
                   << " unchanged=" << summary.unchangedRows << " skipped=" << summary.skippedRows
                   << " duplicates=" << summary.duplicateRows << " conflicts=" << summary.conflicts
                   << " invalid_rows=" << summary.invalidRows
                   << " invalid_number=" << summary.invalidNumberRows
                   << " invalid_description=" << summary.invalidDescriptionRows
                   << " invalid_date=" << summary.invalidDateRows;
            return output.str();
        }

        std::string workflowMessage(const char* operation, const ImportStagingResult& files,
                                    const SsaImportWriteSummary& writeSummary,
                                    const WorkflowFailure& failure = WorkflowFailure{}) {
            const auto failedFiles =
                failure.failedFiles + files.failedCopies + files.failedLegacyXls;
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
                   << " unsupported=" << files.unsupported << " failed=" << failedFiles;
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
            auto message =
                workflowMessage(operation, files, summary, {failedFiles, "operation_failed"});
            if (diagnostic.starts_with("file=")) {
                const auto separator = diagnostic.find(';');
                message += " " + diagnostic.substr(0, separator);
            }
            return {ports::WorkflowStatus::Failed, std::move(message), false,
                    std::move(diagnostic)};
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
        std::vector<domain::ColumnDef> columns, const bool consolidateSources,
        SynchronizationSignals synchronization)
        : inputFolder_(inputFolder), databasePath_(databasePath), columns_(columns),
          consolidateSources_(consolidateSources),
          stager_(inputFolder_, synchronization.afterFirstChunkWritten),
          consolidator_(inputFolder_), synchronization_(std::move(synchronization)),
          writer_(sqlite::SqliteSsaImportWriterAccess{}, databasePath, std::move(columns),
                  "ssa_table", {.busyEntered = synchronization_.writerBusyEntered}) {
        if (const auto resolved = resolvedImportFolder(inputFolder_, importLockPathDiagnostic_)) {
            inputFolder_ = *resolved;
            importLockPath_ = inputFolder_.parent_path() / ".ssa_import.lock";
            stager_ = ImportFileStager(inputFolder_, synchronization_.afterFirstChunkWritten);
            consolidator_ = ImportFileConsolidator(inputFolder_);
        }
    }

    std::optional<ports::WorkflowResult> SpreadsheetImportWorkflowPort::resumePendingConsolidation(
        const std::stop_token& stopToken, const std::chrono::milliseconds sqliteBusyWait,
        const ImportStagingResult* const selectedStaging,
        std::vector<std::size_t>* const selectedPendingSummaryIndices) const {
        std::vector<ImportConsolidationMove> pending;
        try {
            const auto lookupToken = stopToken.stop_requested() ? std::stop_token{} : stopToken;
            const auto lookupBusyWait =
                stopToken.stop_requested() && sqliteBusyWait > std::chrono::milliseconds{250}
                    ? std::chrono::milliseconds{250}
                    : sqliteBusyWait;
            pending = writer_.pendingConsolidation(lookupToken, lookupBusyWait);
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
        if (selectedStaging != nullptr && selectedPendingSummaryIndices != nullptr) {
            selectedPendingSummaryIndices->reserve(selectedStaging->files.size());
            for (const auto& selected : selectedStaging->files) {
                for (const auto& move : pending) {
                    std::string matchingDiagnostic;
                    const auto comparison =
                        compareSelectedStaging(selected, move, stopToken, matchingDiagnostic);
                    if (comparison == FileContentsComparison::Canceled) {
                        return ports::WorkflowResult{ports::WorkflowStatus::Canceled,
                                                     "import_consolidation_resume canceled"};
                    }
                    if (comparison == FileContentsComparison::Failed) {
                        return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                                     "import_consolidation_resume_failed", false,
                                                     std::move(matchingDiagnostic)};
                    }
                    if (comparison == FileContentsComparison::Same) {
                        selectedPendingSummaryIndices->push_back(selected.summaryIndex);
                        break;
                    }
                }
            }
        }

        ImportConsolidationPlan plan;
        plan.entries.reserve(pending.size());
        for (auto& move : pending) {
            plan.entries.push_back({{std::move(move)}});
        }
        const auto consolidation = consolidator_.consolidate(plan, stopToken);
        std::vector<ImportConsolidationMove> completedMoves;
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
                completedMoves.push_back(planEntry.moves[moveIndex]);
            }
        }
        try {
            writer_.completeConsolidation(completedMoves, sqliteBusyWait);
        } catch (const ports::OperationError& error) {
            journalFailures = completedMoves.size();
            appendWorkflowDiagnostic(diagnostic, error.diagnostic());
        } catch (const std::exception& error) {
            journalFailures = completedMoves.size();
            appendWorkflowDiagnostic(diagnostic, error.what());
        }
        const auto completed = journalFailures == 0 ? completedMoves.size() : std::size_t{0};
        if (consolidation.canceled) {
            return ports::WorkflowResult{
                ports::WorkflowStatus::Succeeded,
                "import_consolidation_resume canceled completed=" + std::to_string(completed) +
                    " pending=" + std::to_string(pending.size() - completed),
                true, std::move(diagnostic)};
        }
        if (consolidation.failed > 0 || journalFailures > 0) {
            return ports::WorkflowResult{
                completed > 0 ? ports::WorkflowStatus::Succeeded : ports::WorkflowStatus::Failed,
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
        const ProgressContext globalProgress{&request.progress, 0, request.files.size(), 0, false};
        const auto complete = [&](ports::WorkflowResult&& result) {
            const auto level = result.ok() ? result.warning
                                                 ? ports::WorkflowProgressLevel::Warning
                                                 : ports::WorkflowProgressLevel::Information
                               : result.status == ports::WorkflowStatus::Canceled
                                   ? ports::WorkflowProgressLevel::Warning
                                   : ports::WorkflowProgressLevel::Error;
            reportProgress(globalProgress, ports::WorkflowProgressStage::Completed, level,
                           request.files.size(), 100, result.message, result.diagnostic);
            return std::move(result);
        };
        if (request.files.size() <= kMaxImportFilesPerBatch) {
            return complete(importExternalFilesBatch(
                request, stopToken, {&request.progress, 0, request.files.size(), 1, false}));
        }

        ports::ImportSummary totalSummary;
        totalSummary.files.reserve(request.files.size());
        std::string diagnostic;
        bool warning = false;
        bool anySuccess = false;
        bool hadFailure = false;
        bool hadRejection = false;
        std::size_t completedBatchCount = 0;
        for (std::size_t batchStart = 0; batchStart < request.files.size();
             batchStart += kMaxImportFilesPerBatch) {
            const auto batchEnd =
                (std::min)(request.files.size(), batchStart + kMaxImportFilesPerBatch);
            ports::ImportExternalFilesRequest batchRequest;
            batchRequest.execution = request.execution;
            batchRequest.files.assign(
                request.files.begin() + static_cast<std::ptrdiff_t>(batchStart),
                request.files.begin() + static_cast<std::ptrdiff_t>(batchEnd));
            const ProgressContext progress{&request.progress, batchStart, request.files.size(),
                                           batchStart / kMaxImportFilesPerBatch + 1, false};
            const auto batchResult = importExternalFilesBatch(batchRequest, stopToken, progress);
            ++completedBatchCount;
            if (batchResult.importSummary) {
                mergeImportSummary(totalSummary, *batchResult.importSummary);
            }
            if (!batchResult.diagnostic.empty()) {
                appendWorkflowDiagnostic(diagnostic,
                                         "batch=" + std::to_string(completedBatchCount) + " " +
                                             batchResult.diagnostic);
            }
            warning = warning || batchResult.warning;
            anySuccess = anySuccess || batchResult.ok();
            hadFailure = hadFailure || batchResult.status == ports::WorkflowStatus::Failed;
            hadRejection = hadRejection || batchResult.status == ports::WorkflowStatus::Rejected;
            if (batchResult.status == ports::WorkflowStatus::Canceled ||
                stopToken.stop_requested()) {
                for (std::size_t index = batchEnd; index < request.files.size(); ++index) {
                    ++totalSummary.discovered;
                    ++totalSummary.preserved;
                    totalSummary.files.push_back(
                        {.source = qt::toUtf8(request.files[index].filename()),
                         .status = ports::ImportFileStatus::Canceled});
                }
                return complete(withSummary(
                    {ports::WorkflowStatus::Canceled,
                     "import_xlsx_to_sqlite canceled batch=" + std::to_string(completedBatchCount),
                     true, std::move(diagnostic)},
                    totalSummary));
            }
        }
        const auto status = hadFailure   ? ports::WorkflowStatus::Failed
                            : anySuccess ? ports::WorkflowStatus::Succeeded
                                         : ports::WorkflowStatus::Rejected;
        warning = warning || hadFailure || hadRejection;
        return complete(
            withSummary({status, batchedImportMessage(totalSummary, completedBatchCount), warning,
                         std::move(diagnostic)},
                        totalSummary));
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importExternalFilesBatch(
        const ports::ImportExternalFilesRequest& request, const std::stop_token& stopToken,
        const ProgressContext& progress) const {
        if (progress.batchIndex == 1) {
            reportProgress(progress, ports::WorkflowProgressStage::Preparing,
                           ports::WorkflowProgressLevel::Information, progress.fileOffset, 0,
                           "Preparando importacao");
        }
        if (const auto validation = request.execution.validationError(); !validation.empty()) {
            return withSummary(
                {ports::WorkflowStatus::Rejected,
                 "import_external_files invalid_import_execution_options " + validation},
                selectedFilesSummary(request.files, ports::ImportFileStatus::Rejected));
        }
        if (request.files.empty()) {
            return withSummary(
                {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"}, {});
        }
        if (!importLockPathDiagnostic_.empty()) {
            return importLockFailure(
                QLockFile::UnknownError,
                selectedFilesSummary(request.files, ports::ImportFileStatus::Failed),
                importLockPathDiagnostic_);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        std::string lockDiagnostic;
        auto lockFailureOrigin = ImportLockFailureOrigin::Corpus;
        const auto importLock = acquireImportLocks(importLockPath_, databasePath_, lockError,
                                                   lockDiagnostic, lockFailureOrigin);
        if (!importLock) {
            return importLockFailure(
                lockError, selectedFilesSummary(request.files, ports::ImportFileStatus::Failed),
                lockDiagnostic, lockFailureOrigin);
        }
        auto staging = stager_.stageExternalFiles(request.files, stopToken);
        if (progress.batchIndex == 1) {
            reportProgress(progress, ports::WorkflowProgressStage::Discovering,
                           ports::WorkflowProgressLevel::Information, progress.fileOffset, 5,
                           "Arquivos preparados");
        }
        std::vector<std::size_t> selectedPendingSummaryIndices;
        if (progress.batchIndex == 1) {
            reportProgress(progress, ports::WorkflowProgressStage::Consolidating,
                           ports::WorkflowProgressLevel::Information, progress.fileOffset, 5,
                           "Verificando consolidacao pendente");
        }
        if (auto resumed = resumePendingConsolidation(stopToken, request.execution.sqliteBusyWait,
                                                      &staging, &selectedPendingSummaryIndices)) {
            if (resumed->status == ports::WorkflowStatus::Canceled) {
                return importDiscoveredFiles(staging, false, stopToken, request.execution, nullptr,
                                             &progress);
            }
            if (!resumed->ok() || resumed->warning) {
                const auto cleanupDiagnostic = stager_.discardOwnedArtifacts(staging);
                if (!cleanupDiagnostic.empty()) {
                    resumed->status = ports::WorkflowStatus::Failed;
                    resumed->message = "import_xlsx_to_sqlite staging_cleanup_failed";
                    appendWorkflowDiagnostic(resumed->diagnostic, cleanupDiagnostic);
                }
                if (!resumed->importSummary) {
                    auto summary = makeImportSummary(staging);
                    markUncommitted(summary, ports::ImportFileStatus::Failed);
                    return withSummary(std::move(*resumed), summary);
                }
                return std::move(*resumed);
            }
            ImportStagingResult pendingSelection;
            for (const auto& file : staging.files) {
                if (std::ranges::find(selectedPendingSummaryIndices, file.summaryIndex) !=
                    selectedPendingSummaryIndices.end()) {
                    pendingSelection.files.push_back(file);
                }
            }
            const auto cleanupDiagnostic = stager_.discardOwnedArtifacts(pendingSelection);
            if (!cleanupDiagnostic.empty()) {
                resumed->status = ports::WorkflowStatus::Failed;
                resumed->message = "import_xlsx_to_sqlite staging_cleanup_failed";
                appendWorkflowDiagnostic(resumed->diagnostic, cleanupDiagnostic);
                return std::move(*resumed);
            }
            removeStagedSummaryIndices(staging, selectedPendingSummaryIndices);
            if (staging.discovered == 0) {
                return std::move(*resumed);
            }
            return importDiscoveredFiles(staging, false, stopToken, request.execution, nullptr,
                                         &progress);
        }
        return importDiscoveredFiles(staging, false, stopToken, request.execution, nullptr,
                                     &progress);
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::importSamArtifacts(const ports::SamImportRequest& request,
                                                      const std::stop_token stopToken) {
        std::vector<std::filesystem::path> files;
        files.reserve(request.artifacts.size());
        for (const auto& artifact : request.artifacts) {
            files.push_back(artifact.path);
        }
        ports::ImportExecutionOptions execution;
        execution.rowsPerChunk = request.rowsPerChunk;
        execution.sqliteBusyWait = request.sqliteBusyWait;
        if (const auto validation = execution.validationError(); !validation.empty()) {
            return withSummary({ports::WorkflowStatus::Rejected,
                                "sam_import invalid_import_execution_options " + validation},
                               selectedFilesSummary(files, ports::ImportFileStatus::Rejected));
        }
        if (request.artifacts.empty()) {
            return withSummary({ports::WorkflowStatus::Rejected, "sam_import no_artifacts"}, {});
        }
        if (!importLockPathDiagnostic_.empty()) {
            return importLockFailure(QLockFile::UnknownError,
                                     selectedFilesSummary(files, ports::ImportFileStatus::Failed),
                                     importLockPathDiagnostic_);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        std::string lockDiagnostic;
        auto lockFailureOrigin = ImportLockFailureOrigin::Corpus;
        const auto importLock = acquireImportLocks(importLockPath_, databasePath_, lockError,
                                                   lockDiagnostic, lockFailureOrigin);
        if (!importLock) {
            return importLockFailure(lockError,
                                     selectedFilesSummary(files, ports::ImportFileStatus::Failed),
                                     lockDiagnostic, lockFailureOrigin);
        }
        if (auto resumed = resumePendingConsolidation(stopToken, execution.sqliteBusyWait)) {
            if (!resumed->ok()) {
                return std::move(*resumed);
            }
            if (resumed->warning) {
                resumed->status = ports::WorkflowStatus::Canceled;
                resumed->message = "sam_import canceled_during_consolidation_resume";
                return std::move(*resumed);
            }
        }
        auto staging = stager_.stageExternalFiles(files, stopToken);
        if (staging.files.size() != request.artifacts.size() || staging.failedCopies > 0 ||
            staging.legacyXls > 0 || staging.unsupported > 0) {
            staging.operationalFailure = staging.failedCopies > 0;
            if (staging.rejectionReason.empty()) {
                staging.rejectionReason = "sam_staging_incomplete";
            }
        }
        return importDiscoveredFiles(staging, false, stopToken, execution, &request.artifacts);
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::rescan(const ports::RescanRequest& request,
                                                                const std::stop_token stopToken) {
        ProgressContext progress{&request.progress, 0, 0, 1, true};
        auto result = rescanInternal(request, stopToken, progress);
        const auto level = result.ok() ? result.warning ? ports::WorkflowProgressLevel::Warning
                                                        : ports::WorkflowProgressLevel::Information
                           : result.status == ports::WorkflowStatus::Canceled
                               ? ports::WorkflowProgressLevel::Warning
                               : ports::WorkflowProgressLevel::Error;
        reportProgress(progress, ports::WorkflowProgressStage::Completed, level,
                       progress.totalFiles, 100, result.message, result.diagnostic);
        return result;
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::rescanInternal(const ports::RescanRequest& request,
                                                  const std::stop_token& stopToken,
                                                  ProgressContext& progress) {
        reportProgress(progress, ports::WorkflowProgressStage::Preparing,
                       ports::WorkflowProgressLevel::Information, 0, 0,
                       "Preparando reescaneamento");
        if (const auto validation = request.execution.validationError(); !validation.empty()) {
            return withSummary({ports::WorkflowStatus::Rejected,
                                "rescan invalid_import_execution_options " + validation},
                               {});
        }
        if (stopToken.stop_requested()) {
            return withSummary(canceled("rescan"), {});
        }
        const bool replaceAll = request.mode == ports::RescanMode::Full;
        const auto directoryStatus = stager_.validateInputDirectory(stopToken);
        if (!directoryStatus.rejectionReason.empty()) {
            return importDiscoveredFiles(directoryStatus, replaceAll, stopToken, request.execution);
        }
        if (!importLockPathDiagnostic_.empty()) {
            return importLockFailure(QLockFile::UnknownError, {}, importLockPathDiagnostic_);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        std::string lockDiagnostic;
        auto lockFailureOrigin = ImportLockFailureOrigin::Corpus;
        const auto importLock = acquireImportLocks(importLockPath_, databasePath_, lockError,
                                                   lockDiagnostic, lockFailureOrigin);
        if (!importLock) {
            return importLockFailure(lockError, {}, lockDiagnostic, lockFailureOrigin);
        }
        reportProgress(progress, ports::WorkflowProgressStage::Preparing,
                       ports::WorkflowProgressLevel::Information, 0, 3,
                       "Verificando consolidacao pendente");
        auto resumed = resumePendingConsolidation(stopToken, request.execution.sqliteBusyWait);
        if (resumed && (resumed->status == ports::WorkflowStatus::Canceled || !resumed->ok() ||
                        resumed->warning)) {
            return std::move(*resumed);
        }
        auto staging = stager_.stageInputFiles(stopToken, replaceAll);
        progress.totalFiles = staging.files.size();
        reportProgress(progress, ports::WorkflowProgressStage::Discovering,
                       ports::WorkflowProgressLevel::Information, 0, 5,
                       "Arquivos descobertos: " + std::to_string(progress.totalFiles));
        if (staging.operationalFailure || !staging.rejectionReason.empty()) {
            return importDiscoveredFiles(staging, replaceAll, stopToken, request.execution);
        }
        if (resumed && staging.files.empty()) {
            return std::move(*resumed);
        }
        if (staging.files.empty()) {
            return importDiscoveredFiles(staging, replaceAll, stopToken, request.execution);
        }
        const auto uncommittedResult =
            [&](ports::WorkflowResult result, const ports::ImportFileStatus fileStatus,
                const ports::ImportSummary* const candidateSummary = nullptr) {
                auto summary =
                    candidateSummary == nullptr ? makeImportSummary(staging) : *candidateSummary;
                markUncommitted(summary, fileStatus);
                return withSummary(std::move(result), summary);
            };
        std::error_code databaseDirectoryError;
        const auto databaseDirectory = databasePath_.parent_path();
        const auto databaseDirectoryStatus =
            std::filesystem::status(databaseDirectory, databaseDirectoryError);
        if (databaseDirectoryError || !std::filesystem::is_directory(databaseDirectoryStatus)) {
            const auto diagnostic =
                databaseDirectoryError
                    ? "cannot access database target directory: " + databaseDirectoryError.message()
                    : "database target path is not a directory";
            return uncommittedResult({ports::WorkflowStatus::Failed,
                                      "rescan database snapshot failed", false, diagnostic},
                                     ports::ImportFileStatus::Failed);
        }
        QTemporaryDir workingDirectory;
        if (!workingDirectory.isValid()) {
            return uncommittedResult(
                {ports::WorkflowStatus::Failed, "rescan working directory unavailable"},
                ports::ImportFileStatus::Failed);
        }
        const auto workingDatabase =
            qt::toFileSystemPath(workingDirectory.path()) / databasePath_.filename();
        try {
            reportProgress(progress, ports::WorkflowProgressStage::CopyingDatabase,
                           ports::WorkflowProgressLevel::Information, 0, 8,
                           "Copiando banco para validacao");
            backupDatabaseSnapshot(
                databasePath_, workingDatabase, stopToken, request.execution.sqliteBusyWait,
                DatabaseSnapshotPhase::InitialCopy, synchronization_.snapshotLocked.get());
            SpreadsheetImportWorkflowPort workingPort(inputFolder_, workingDatabase, columns_,
                                                      false, synchronization_);
            auto result = workingPort.importDiscoveredFiles(staging, replaceAll, stopToken,
                                                            request.execution, nullptr, &progress);
            if (!result.ok()) {
                return result;
            }
            if (stopToken.stop_requested()) {
                return uncommittedResult(canceled("rescan"), ports::ImportFileStatus::Canceled,
                                         result.importSummary ? &*result.importSummary : nullptr);
            }
            if (!result.importSummary) {
                result.warning = true;
                appendWorkflowDiagnostic(result.diagnostic, "rescan summary unavailable");
                return result;
            }
            std::vector<ImportManifestEntry> manifest;
            manifest.reserve(staging.files.size());
            std::vector<const StagedImportFile*> consolidableFiles;
            consolidableFiles.reserve(staging.files.size());
            for (const auto& file : staging.files) {
                const auto& fileResult = result.importSummary->files[file.summaryIndex];
                if (fileResult.status == ports::ImportFileStatus::Rejected ||
                    fileResult.status == ports::ImportFileStatus::Ignored) {
                    continue;
                }
                const bool hasValidRows = fileResult.status == ports::ImportFileStatus::Applied ||
                                          fileResult.status == ports::ImportFileStatus::NoChanges;
                manifest.push_back(
                    {file.consolidationSources, hasValidRows, file.consolidationFilename});
                consolidableFiles.push_back(&file);
            }
            const auto consolidationPlan = consolidator_.plan(manifest, stopToken);
            if (consolidationPlan.canceled || !consolidationPlan.error.empty()) {
                const auto canceledPlan = consolidationPlan.canceled;
                return uncommittedResult(
                    {canceledPlan ? ports::WorkflowStatus::Canceled : ports::WorkflowStatus::Failed,
                     canceledPlan ? "rescan canceled error=consolidation_canceled"
                                  : "rescan error=consolidation_failed",
                     canceledPlan,
                     canceledPlan ? "rescan consolidation canceled" : consolidationPlan.error},
                    canceledPlan ? ports::ImportFileStatus::Canceled
                                 : ports::ImportFileStatus::Failed,
                    &*result.importSummary);
            }
            reportProgress(progress, ports::WorkflowProgressStage::PublishingDatabase,
                           ports::WorkflowProgressLevel::Information, progress.totalFiles, 90,
                           "Publicando banco validado");
            backupDatabaseSnapshot(
                workingDatabase, databasePath_, stopToken, request.execution.sqliteBusyWait,
                DatabaseSnapshotPhase::Publication, synchronization_.snapshotLocked.get());
            reportProgress(progress, ports::WorkflowProgressStage::Consolidating,
                           ports::WorkflowProgressLevel::Information, progress.totalFiles, 95,
                           "Consolidando arquivos processados");
            const auto consolidation = consolidator_.consolidate(consolidationPlan, stopToken);
            appendWorkflowDiagnostic(result.diagnostic, consolidation.error);
            result.warning = result.warning || consolidation.canceled || consolidation.failed > 0;
            std::vector<ImportConsolidationMove> completedMoves;
            std::size_t journalFailures = 0;
            std::size_t completedFiles = 0;
            for (std::size_t index = 0; index < consolidationPlan.entries.size(); ++index) {
                if (index >= consolidation.entries.size() ||
                    consolidation.entries[index].failed > 0 ||
                    consolidation.entries[index].completed != manifest[index].sources.size()) {
                    continue;
                }
                auto& fileResult =
                    result.importSummary->files[consolidableFiles[index]->summaryIndex];
                fileResult.consolidated = true;
                fileResult.noSurvivor = !manifest[index].hasValidRows;
                ++completedFiles;
                result.importSummary->consolidated += manifest[index].hasValidRows ? 1 : 0;
                result.importSummary->noSurvivor += manifest[index].hasValidRows ? 0 : 1;
            }
            result.importSummary->preserved =
                result.importSummary->files.size() > completedFiles
                    ? result.importSummary->files.size() - completedFiles
                    : 0;
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
                    completedMoves.push_back(planEntry.moves[moveIndex]);
                }
            }
            try {
                writer_.completeConsolidation(completedMoves, request.execution.sqliteBusyWait);
            } catch (const ports::OperationError& error) {
                journalFailures = completedMoves.size();
                appendWorkflowDiagnostic(result.diagnostic, error.diagnostic());
            } catch (const std::exception& error) {
                journalFailures = completedMoves.size();
                appendWorkflowDiagnostic(result.diagnostic, error.what());
            }
            if (consolidation.canceled || consolidation.failed > 0 || journalFailures > 0) {
                result.status = ports::WorkflowStatus::Succeeded;
                result.warning = true;
                result.message += consolidation.canceled ? " error=consolidation_canceled"
                                                         : " error=consolidation_failed";
                return result;
            }
            return result;
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return uncommittedResult(canceled("rescan"), ports::ImportFileStatus::Canceled);
            }
            return uncommittedResult({ports::WorkflowStatus::Failed,
                                      "rescan database snapshot failed", false, error.what()},
                                     ports::ImportFileStatus::Failed);
        } catch (const ports::OperationError& error) {
            return uncommittedResult({ports::WorkflowStatus::Failed,
                                      "rescan database snapshot failed", false, error.diagnostic()},
                                     ports::ImportFileStatus::Failed);
        } catch (const std::exception& error) {
            return uncommittedResult({ports::WorkflowStatus::Failed,
                                      "rescan database snapshot failed", false, error.what()},
                                     ports::ImportFileStatus::Failed);
        }
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importDiscoveredFiles(
        const ImportStagingResult& files, const bool replaceAll, const std::stop_token& stopToken,
        const ports::ImportExecutionOptions& execution,
        const std::vector<ports::SamArtifact>* samArtifacts,
        const ProgressContext* const progress) const {
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
            if (files.warning || !cleanupDiagnostic.empty()) {
                result.warning = true;
            }
            const auto fileStatus = result.status == ports::WorkflowStatus::Canceled
                                        ? ports::ImportFileStatus::Canceled
                                    : result.status == ports::WorkflowStatus::Rejected
                                        ? ports::ImportFileStatus::Rejected
                                        : ports::ImportFileStatus::Failed;
            markUncommitted(importSummary, fileStatus);
            return withSummary(std::move(result), importSummary);
        };
        if (stopToken.stop_requested()) {
            return discardBeforeCommit(canceled(operation));
        }
        if (files.rejectionReason == "staging_cleanup_failed") {
            markUncommitted(importSummary, ports::ImportFileStatus::Failed);
            ports::WorkflowResult result{ports::WorkflowStatus::Failed,
                                         "import_xlsx_to_sqlite staging_cleanup_failed", false,
                                         files.diagnostic};
            return withSummary(std::move(result), importSummary);
        }
        if (files.operationalFailure && files.files.empty()) {
            const auto cause =
                files.rejectionReason.empty() ? "staging_failed" : files.rejectionReason;
            return discardBeforeCommit(
                {ports::WorkflowStatus::Failed, std::string{operation} + " " + cause});
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
        bool ignoredUnrecognizedWorkbook = false;
        std::unique_ptr<sqlite::SqliteSsaImportWriter::WriteSession> writeSession;
        try {
            writeSession = std::make_unique<sqlite::SqliteSsaImportWriter::WriteSession>(
                writer_.startSession(replaceAll, stopToken, execution.sqliteBusyWait));
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
        std::vector<PendingImportOutcome> pendingOutcomes;
        pendingOutcomes.reserve(files.files.size());
        for (std::size_t fileIndex = 0; fileIndex < files.files.size(); ++fileIndex) {
            const auto& file = files.files[fileIndex];
            auto& fileResult = importSummary.files[file.summaryIndex];
            if (progress != nullptr) {
                const auto currentFile = progress->fileOffset + fileIndex + 1;
                const auto percentage =
                    progress->totalFiles == 0
                        ? 10
                        : 10 + static_cast<int>(70 * currentFile / progress->totalFiles);
                reportProgress(*progress, ports::WorkflowProgressStage::ProcessingFile,
                               ports::WorkflowProgressLevel::Information, currentFile, percentage,
                               "Arquivo " + std::to_string(currentFile) + "/" +
                                   std::to_string(progress->totalFiles),
                               "Processando: " + file.originalFilename, file.originalFilename);
            }
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            SsaImportBatch batch;
            std::size_t validRows = 0;
            bool invalidFullBatch = false;
            bool duplicateConflict = false;
            std::string samRejectionReason;
            ChunkedWorkbookImportResult chunkedWorkbookResult;
            bool hasChunkedWorkbookResult = false;
            bool appliedChunkedWorkbookResult = false;
            const auto applyChunkedWorkbookResult = [&] {
                if (!hasChunkedWorkbookResult || appliedChunkedWorkbookResult) {
                    return;
                }
                batch = std::move(chunkedWorkbookResult.batch);
                validRows = chunkedWorkbookResult.validRows;
                invalidFullBatch = chunkedWorkbookResult.invalidFullBatch;
                duplicateConflict = chunkedWorkbookResult.duplicateConflict;
                fileResult.conflicts += chunkedWorkbookResult.conflictRows +
                                        chunkedWorkbookResult.writeSummary.conflictRows;
                fileResult.inserts += chunkedWorkbookResult.writeSummary.rowsInserted;
                fileResult.updates += chunkedWorkbookResult.writeSummary.rowsUpdated;
                fileResult.unchangedRows += chunkedWorkbookResult.writeSummary.rowsUnchanged;
                importSummary.conflicts += chunkedWorkbookResult.conflictRows +
                                           chunkedWorkbookResult.writeSummary.conflictRows;
                importSummary.inserts += chunkedWorkbookResult.writeSummary.rowsInserted;
                importSummary.updates += chunkedWorkbookResult.writeSummary.rowsUpdated;
                importSummary.unchangedRows += chunkedWorkbookResult.writeSummary.rowsUnchanged;
                totalSummary.conflictRows += chunkedWorkbookResult.conflictRows +
                                             chunkedWorkbookResult.writeSummary.conflictRows;
                appliedChunkedWorkbookResult = true;
            };
            try {
                if (samArtifacts != nullptr) {
                    auto adapted = readAndAdaptSamWorkbook(
                        file, samArtifacts->at(file.summaryIndex), stopToken);
                    if (!adapted.ok()) {
                        samRejectionReason = std::move(adapted.rejectionReason);
                    } else {
                        batch = SsaSpreadsheetMapper::map(adapted.table, stopToken);
                        validRows = batch.rows.size();
                        if (batch.mappingStatus == SpreadsheetMappingStatus::Mapped &&
                            !batch.rows.empty()) {
                            const std::vector<SsaImportBatch> mapped{batch};
                            const auto resolved =
                                conflictResolver_.resolveBySsaNumberKeepingUnkeyedRows(mapped);
                            fileResult.conflicts += resolved.conflictRows;
                            importSummary.conflicts += resolved.conflictRows;
                            totalSummary.conflictRows += resolved.conflictRows;
                            if (resolved.conflictRows > 0) {
                                duplicateConflict = true;
                            } else {
                                const auto batchWrite = writeSession->write(resolved, 1, 0);
                                fileResult.inserts += batchWrite.rowsInserted;
                                fileResult.updates += batchWrite.rowsUpdated;
                                fileResult.unchangedRows += batchWrite.rowsUnchanged;
                                fileResult.conflicts += batchWrite.conflictRows;
                                importSummary.inserts += batchWrite.rowsInserted;
                                importSummary.updates += batchWrite.rowsUpdated;
                                importSummary.unchangedRows += batchWrite.rowsUnchanged;
                                importSummary.conflicts += batchWrite.conflictRows;
                                totalSummary.conflictRows += batchWrite.conflictRows;
                                duplicateConflict = batchWrite.conflictRows > 0;
                            }
                        }
                    }
                } else {
                    hasChunkedWorkbookResult = true;
                    readAndImportChunkedWorkbook(file, replaceAll, conflictResolver_, *writeSession,
                                                 execution.rowsPerChunk, stopToken,
                                                 chunkedWorkbookResult);
                    applyChunkedWorkbookResult();
                }
            } catch (const std::system_error& error) {
                applyChunkedWorkbookResult();
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
                }
                fileResult.reason = "operation_failed";
                ++failedFiles;
                return discardBeforeCommit(rollbackSession(
                    *writeSession, failed(operation, files, totalSummary, failedFiles,
                                          "file=" + file.originalFilename + "; " + error.what())));
            } catch (const ports::OperationError& error) {
                applyChunkedWorkbookResult();
                fileResult.reason = "operation_failed";
                ++failedFiles;
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    failed(operation, files, totalSummary, failedFiles,
                           "file=" + file.originalFilename + "; " + error.diagnostic())));
            } catch (const std::exception& exc) {
                applyChunkedWorkbookResult();
                fileResult.reason = "operation_failed";
                ++failedFiles;
                return discardBeforeCommit(rollbackSession(
                    *writeSession, failed(operation, files, totalSummary, failedFiles,
                                          "file=" + file.originalFilename + "; " + exc.what())));
            }
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            if (!samRejectionReason.empty()) {
                fileResult.reason = "sam_rejected";
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " " + samRejectionReason}));
            }
            fileResult.validRows = validRows;
            fileResult.invalidRows = batch.invalidRows;
            fileResult.invalidNumberRows = batch.invalidNumberRows;
            fileResult.invalidDescriptionRows = batch.invalidDescriptionRows;
            fileResult.invalidDateRows = batch.invalidDateRows;
            importSummary.validRows += validRows;
            importSummary.invalidRows += batch.invalidRows;
            importSummary.invalidNumberRows += batch.invalidNumberRows;
            importSummary.invalidDescriptionRows += batch.invalidDescriptionRows;
            importSummary.invalidDateRows += batch.invalidDateRows;
            importSummary.skippedRows += batch.skippedRows;
            totalSummary.skippedRows += batch.skippedRows;
            totalSummary.invalidRows += batch.invalidRows;
            totalSummary.invalidNumberRows += batch.invalidNumberRows;
            totalSummary.invalidDescriptionRows += batch.invalidDescriptionRows;
            totalSummary.invalidDateRows += batch.invalidDateRows;
            if (duplicateConflict) {
                fileResult.reason = "duplicate_conflict";
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     workflowMessage(operation, files, totalSummary, {0, "duplicate_conflict"})}));
            }
            if (batch.mappingStatus == SpreadsheetMappingStatus::HeaderNotRecognized) {
                fileResult.status = ports::ImportFileStatus::Ignored;
                fileResult.reason = "header_not_recognized";
                ++importSummary.ignored;
                ++importSummary.preserved;
                ignoredUnrecognizedWorkbook = true;
                continue;
            }
            if (batch.mappingStatus == SpreadsheetMappingStatus::RequiredColumnsMissing) {
                fileResult.reason = "required_columns_missing";
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " required_columns_missing file=" +
                                        file.originalFilename}));
            }
            if (batch.mappingStatus == SpreadsheetMappingStatus::AmbiguousHeaders) {
                fileResult.reason = "ambiguous_headers";
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     std::string{operation} + " ambiguous_headers file=" + file.originalFilename}));
            }
            if (invalidFullBatch) {
                fileResult.reason = "invalid_rows";
                std::ostringstream invalidDiagnostic;
                invalidDiagnostic << "invalid_rows file=" << file.originalFilename
                                  << " invalid_number=" << batch.invalidNumberRows
                                  << " invalid_description=" << batch.invalidDescriptionRows
                                  << " invalid_date=" << batch.invalidDateRows;
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    workflowMessage(operation, files, totalSummary,
                                                    {0, invalidDiagnostic.str()})}));
            }
            pendingOutcomes.push_back({&file, validRows > 0, file.summaryIndex});
            if (validRows == 0) {
                fileResult.status = ports::ImportFileStatus::NoValidRows;
                if (replaceAll) {
                    fileResult.reason = "no_valid_rows";
                    return discardBeforeCommit(rollbackSession(
                        *writeSession, {ports::WorkflowStatus::Rejected,
                                        std::string{operation} + " no_valid_rows"}));
                }
                continue;
            }
            fileResult.status = fileResult.inserts + fileResult.updates > 0
                                    ? ports::ImportFileStatus::Applied
                                    : ports::ImportFileStatus::NoChanges;
        }
        if (pendingOutcomes.empty() ||
            std::ranges::none_of(pendingOutcomes, [](const PendingImportOutcome& outcome) {
                return outcome.hasValidRows;
            })) {
            return discardBeforeCommit(rollbackSession(
                *writeSession,
                {ports::WorkflowStatus::Rejected,
                 workflowMessage(operation, files, totalSummary, {0, "no_valid_rows"})}));
        }
        std::vector<ImportManifestEntry> manifest;
        manifest.reserve(pendingOutcomes.size());
        for (const auto& outcome : pendingOutcomes) {
            manifest.push_back({outcome.file->consolidationSources, outcome.hasValidRows,
                                outcome.file->consolidationFilename});
        }
        const auto consolidationPlan = consolidator_.plan(manifest, stopToken);
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
            int observedIsoYear = 0;
            const auto observedDate = QDate::currentDate();
            const int observedIsoWeek = observedDate.weekNumber(&observedIsoYear);
            if (progress != nullptr) {
                const auto currentFile = progress->fileOffset + files.files.size();
                reportProgress(*progress, ports::WorkflowProgressStage::UpdatingAnalytics,
                               ports::WorkflowProgressLevel::Information, currentFile,
                               progress->rescan ? 82 : -1, "Atualizando indicadores");
            }
            const auto writeSummary = writeSession->finishWithAnalytics(
                observedIsoYear * domain::kYearWeekMultiplier + observedIsoWeek,
                observedDate.toString(Qt::ISODate).toStdString());
            totalSummary.rowsWritten = writeSummary.rowsWritten;
            totalSummary.rowsInserted = writeSummary.rowsInserted;
            totalSummary.rowsUpdated = writeSummary.rowsUpdated;
            totalSummary.rowsUnchanged = writeSummary.rowsUnchanged;
            totalSummary.skippedRows += writeSummary.skippedRows;
            totalSummary.duplicateRows = writeSummary.duplicateRows;
            importSummary.inserts = writeSummary.rowsInserted;
            importSummary.updates = writeSummary.rowsUpdated;
            importSummary.unchangedRows = writeSummary.rowsUnchanged;
            importSummary.skippedRows = totalSummary.skippedRows;
            importSummary.duplicateRows = totalSummary.duplicateRows;
            importSummary.invalidNumberRows = totalSummary.invalidNumberRows;
            importSummary.invalidDescriptionRows = totalSummary.invalidDescriptionRows;
            importSummary.invalidDateRows = totalSummary.invalidDateRows;
            if (progress != nullptr) {
                const auto currentFile = progress->fileOffset + files.files.size();
                reportProgress(*progress, ports::WorkflowProgressStage::Committing,
                               ports::WorkflowProgressLevel::Information, currentFile,
                               progress->rescan ? 85 : -1, "Transacao confirmada");
            }
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
        if (!consolidateSources_) {
            importSummary.preserved = importSummary.files.size();
            const auto status = totalSummary.rowsWritten > 0 ? ports::WorkflowStatus::Succeeded
                                                             : ports::WorkflowStatus::NoChanges;
            ports::WorkflowResult result{
                status, workflowMessage(operation, files, totalSummary),
                files.warning || files.failedCopies > 0 || files.failedLegacyXls > 0 ||
                    totalSummary.invalidRows > 0 || totalSummary.duplicateRows > 0 ||
                    ignoredUnrecognizedWorkbook,
                files.diagnostic};
            return withSummary(std::move(result), importSummary);
        }
        if (progress != nullptr) {
            const auto currentFile = progress->fileOffset + files.files.size();
            reportProgress(*progress, ports::WorkflowProgressStage::Consolidating,
                           ports::WorkflowProgressLevel::Information, currentFile,
                           progress->rescan ? 95 : -1, "Consolidando arquivos processados");
        }
        const auto consolidation = consolidator_.consolidate(consolidationPlan, stopToken);
        auto diagnostic = files.diagnostic;
        appendWorkflowDiagnostic(diagnostic, consolidation.error);
        std::vector<ImportConsolidationMove> completedMoves;
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
                completedMoves.push_back(planEntry.moves[moveIndex]);
            }
        }
        try {
            writer_.completeConsolidation(completedMoves, execution.sqliteBusyWait);
        } catch (const ports::OperationError& error) {
            journalFailures = completedMoves.size();
            appendWorkflowDiagnostic(diagnostic, error.diagnostic());
        } catch (const std::exception& error) {
            journalFailures = completedMoves.size();
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
        ports::WorkflowResult result{
            status,
            workflowMessage(operation, files, totalSummary, {failedFiles, consolidationState}),
            files.warning || files.failedCopies > 0 || files.failedLegacyXls > 0 ||
                consolidation.failed > 0 || consolidation.canceled || journalFailures > 0 ||
                totalSummary.invalidRows > 0 || totalSummary.duplicateRows > 0 ||
                ignoredUnrecognizedWorkbook,
            std::move(diagnostic)};
        return withSummary(std::move(result), importSummary);
    }

    void SpreadsheetImportWorkflowPort::reportProgress(const ProgressContext& context,
                                                       const ports::WorkflowProgressStage stage,
                                                       const ports::WorkflowProgressLevel level,
                                                       const std::size_t currentFile,
                                                       const int percentage, std::string status,
                                                       std::string detail,
                                                       std::string fileName) noexcept {
        if (context.callback == nullptr || !*context.callback) {
            return;
        }
        const auto resolvedPercentage =
            percentage >= 0 ? percentage
            : context.totalFiles == 0
                ? 0
                : 10 + static_cast<int>(70 * currentFile / context.totalFiles);
        try {
            (*context.callback)({stage, level, currentFile, context.totalFiles,
                                 (std::clamp)(resolvedPercentage, 0, 100), std::move(fileName),
                                 std::move(status), std::move(detail)});
        } catch (const std::exception& error) {
            qWarning().noquote() << "Workflow progress callback failed:" << error.what();
        } catch (...) {
            qWarning() << "Workflow progress callback failed: unknown exception";
        }
    }

} // namespace ssa::infra::importing
