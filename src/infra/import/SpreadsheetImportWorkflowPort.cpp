#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QFile>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryDir>

#include <sqlite3.h>

#include <chrono>
#include <functional>
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

        constexpr std::size_t kImportRowsPerChunk = 1'000;
        constexpr std::size_t kMaxWorkflowDiagnosticBytes = 4'096;
        constexpr int kDatabaseBackupPagesPerStep = 256;
        constexpr auto kDatabaseBackupRetryDelay = std::chrono::milliseconds{10};
        constexpr auto kDatabaseBackupTimeout = std::chrono::seconds{5};

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

        void copyDatabaseSnapshot(const std::filesystem::path& source,
                                  const std::filesystem::path& destination,
                                  const std::stop_token& stopToken) {
            if (!std::filesystem::exists(source)) {
                return;
            }
            sqlite::SqliteConnection sourceConnection(source, sqlite::SqliteOpenMode::ReadOnly);
            sqlite::SqliteConnection destinationConnection(destination,
                                                           sqlite::SqliteOpenMode::ReadWriteCreate);
            auto* backup = sqlite3_backup_init(destinationConnection.handle(), "main",
                                               sourceConnection.handle(), "main");
            if (backup == nullptr) {
                throw ports::OperationError(
                    "Falha ao preparar copia protegida do banco",
                    sqliteBackupError(destinationConnection.handle(), "sqlite backup init",
                                      sqlite3_errcode(destinationConnection.handle())));
            }
            int stepResult = SQLITE_OK;
            const auto retryDeadline = std::chrono::steady_clock::now() + kDatabaseBackupTimeout;
            while (stepResult == SQLITE_OK || stepResult == SQLITE_BUSY ||
                   stepResult == SQLITE_LOCKED) {
                if (stopToken.stop_requested()) {
                    stepResult = SQLITE_INTERRUPT;
                    break;
                }
                stepResult = sqlite3_backup_step(backup, kDatabaseBackupPagesPerStep);
                if ((stepResult == SQLITE_BUSY || stepResult == SQLITE_LOCKED) &&
                    std::chrono::steady_clock::now() < retryDeadline) {
                    std::this_thread::sleep_for(kDatabaseBackupRetryDelay);
                } else if (stepResult == SQLITE_BUSY || stepResult == SQLITE_LOCKED) {
                    break;
                }
            }
            const int finishResult = sqlite3_backup_finish(backup);
            if (stopToken.stop_requested() || stepResult == SQLITE_INTERRUPT) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "database snapshot canceled");
            }
            if (stepResult != SQLITE_DONE || finishResult != SQLITE_OK) {
                const int errorCode = stepResult != SQLITE_DONE ? stepResult : finishResult;
                throw ports::OperationError(
                    "Falha ao copiar banco para rescan",
                    sqliteBackupError(destinationConnection.handle(), "sqlite backup", errorCode));
            }
        }

        void publishDatabaseSnapshot(const std::filesystem::path& source,
                                     const std::filesystem::path& destination,
                                     const std::stop_token& stopToken) {
            QFile input(qt::toQString(source));
            if (!input.open(QIODevice::ReadOnly)) {
                throw ports::OperationError("Falha ao publicar banco do rescan",
                                            "cannot read database snapshot: " +
                                                input.errorString().toStdString());
            }
            QSaveFile output(qt::toQString(destination));
            if (!output.open(QIODevice::WriteOnly)) {
                throw ports::OperationError("Falha ao publicar banco do rescan",
                                            "cannot open atomic database target: " +
                                                output.errorString().toStdString());
            }
            constexpr qint64 chunkSize = 1024 * 1024;
            while (!input.atEnd()) {
                if (stopToken.stop_requested()) {
                    output.cancelWriting();
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                            "database snapshot canceled");
                }
                const auto chunk = input.read(chunkSize);
                if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
                    output.cancelWriting();
                    throw ports::OperationError("Falha ao publicar banco do rescan",
                                                "cannot read database snapshot: " +
                                                    input.errorString().toStdString());
                }
                if (!chunk.isEmpty() && output.write(chunk) != chunk.size()) {
                    output.cancelWriting();
                    throw ports::OperationError("Falha ao publicar banco do rescan",
                                                "cannot write atomic database target: " +
                                                    output.errorString().toStdString());
                }
            }
            if (stopToken.stop_requested()) {
                output.cancelWriting();
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "database snapshot canceled");
            }
            if (!output.commit()) {
                throw ports::OperationError("Falha ao publicar banco do rescan",
                                            "atomic database publication failed: " +
                                                output.errorString().toStdString());
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
        std::vector<domain::ColumnDef> columns, const bool consolidateSources)
        : inputFolder_(inputFolder), databasePath_(databasePath), columns_(columns),
          consolidateSources_(consolidateSources), stager_(std::move(inputFolder)),
          writer_(sqlite::SqliteSsaImportWriterAccess{}, databasePath, std::move(columns)) {
        if (const auto resolved = resolvedImportFolder(inputFolder_, importLockPathDiagnostic_)) {
            inputFolder_ = *resolved;
            importLockPath_ = inputFolder_.parent_path() / ".ssa_import.lock";
            stager_ = ImportFileStager(inputFolder_);
        }
    }

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
            writer_.completeConsolidation(completedMoves);
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
        if (request.files.empty()) {
            return withSummary(
                {ports::WorkflowStatus::Rejected, "import_external_files no_files_selected"}, {});
        }
        if (!importLockPathDiagnostic_.empty()) {
            return importLockFailure(QLockFile::UnknownError,
                                     selectedFilesFailureSummary(request.files),
                                     importLockPathDiagnostic_);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        std::string lockDiagnostic;
        auto lockFailureOrigin = ImportLockFailureOrigin::Corpus;
        const auto importLock = acquireImportLocks(importLockPath_, databasePath_, lockError,
                                                   lockDiagnostic, lockFailureOrigin);
        if (!importLock) {
            return importLockFailure(lockError, selectedFilesFailureSummary(request.files),
                                     lockDiagnostic, lockFailureOrigin);
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

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::importSamArtifacts(const ports::SamImportRequest& request,
                                                      const std::stop_token stopToken) {
        if (request.artifacts.empty()) {
            return withSummary({ports::WorkflowStatus::Rejected, "sam_import no_artifacts"}, {});
        }
        if (!importLockPathDiagnostic_.empty()) {
            std::vector<std::filesystem::path> files;
            files.reserve(request.artifacts.size());
            for (const auto& artifact : request.artifacts) {
                files.push_back(artifact.path);
            }
            return importLockFailure(QLockFile::UnknownError, selectedFilesFailureSummary(files),
                                     importLockPathDiagnostic_);
        }
        QLockFile::LockError lockError = QLockFile::NoError;
        std::string lockDiagnostic;
        auto lockFailureOrigin = ImportLockFailureOrigin::Corpus;
        const auto importLock = acquireImportLocks(importLockPath_, databasePath_, lockError,
                                                   lockDiagnostic, lockFailureOrigin);
        if (!importLock) {
            std::vector<std::filesystem::path> files;
            files.reserve(request.artifacts.size());
            for (const auto& artifact : request.artifacts) {
                files.push_back(artifact.path);
            }
            return importLockFailure(lockError, selectedFilesFailureSummary(files), lockDiagnostic,
                                     lockFailureOrigin);
        }
        if (auto resumed = resumePendingConsolidation(stopToken)) {
            if (!resumed->ok()) {
                return std::move(*resumed);
            }
            if (resumed->warning) {
                resumed->status = ports::WorkflowStatus::Canceled;
                resumed->message = "sam_import canceled_during_consolidation_resume";
                return std::move(*resumed);
            }
        }
        std::vector<std::filesystem::path> files;
        files.reserve(request.artifacts.size());
        for (const auto& artifact : request.artifacts) {
            files.push_back(artifact.path);
        }
        auto staging = stager_.stageExternalFiles(files, stopToken);
        if (staging.files.size() != request.artifacts.size() || staging.failedCopies > 0 ||
            staging.legacyXls > 0 || staging.unsupported > 0) {
            staging.operationalFailure = staging.failedCopies > 0;
            if (staging.rejectionReason.empty()) {
                staging.rejectionReason = "sam_staging_incomplete";
            }
        }
        return importDiscoveredFiles(staging, false, stopToken, &request.artifacts);
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
        auto staging = stager_.stageInputFiles(stopToken, replaceAll);
        if (staging.operationalFailure || !staging.rejectionReason.empty()) {
            return importDiscoveredFiles(staging, replaceAll, stopToken);
        }
        if (auto resumed = resumePendingConsolidation(stopToken)) {
            if (resumed->status == ports::WorkflowStatus::Canceled) {
                return replaceAll ? importDiscoveredFiles(staging, true, stopToken)
                                  : importIncrementalFiles(staging, stopToken);
            }
            if (!resumed->ok() || resumed->warning) {
                return std::move(*resumed);
            }
            staging = stager_.stageInputFiles(stopToken, replaceAll);
            if (staging.operationalFailure || !staging.rejectionReason.empty()) {
                return importDiscoveredFiles(staging, replaceAll, stopToken);
            }
            if (staging.files.empty()) {
                return std::move(*resumed);
            }
        }
        if (staging.files.empty()) {
            return importDiscoveredFiles(staging, replaceAll, stopToken);
        }
        std::error_code databaseDirectoryError;
        const auto databaseDirectory = databasePath_.parent_path();
        const auto databaseDirectoryStatus =
            std::filesystem::status(databaseDirectory, databaseDirectoryError);
        if (databaseDirectoryError || !std::filesystem::is_directory(databaseDirectoryStatus)) {
            const auto diagnostic =
                databaseDirectoryError
                    ? "cannot access database target directory: " + databaseDirectoryError.message()
                    : "database target path is not a directory";
            return {ports::WorkflowStatus::Failed, "rescan database snapshot failed", false,
                    diagnostic};
        }
        QTemporaryDir workingDirectory;
        if (!workingDirectory.isValid()) {
            return {ports::WorkflowStatus::Failed, "rescan working directory unavailable"};
        }
        const auto workingDatabase =
            qt::toFileSystemPath(workingDirectory.path()) / databasePath_.filename();
        try {
            copyDatabaseSnapshot(databasePath_, workingDatabase, stopToken);
            SpreadsheetImportWorkflowPort workingPort(inputFolder_, workingDatabase, columns_,
                                                      false);
            auto result = workingPort.importDiscoveredFiles(staging, replaceAll, stopToken);
            if (!result.ok()) {
                return result;
            }
            if (stopToken.stop_requested()) {
                return canceled("rescan");
            }
            if (!result.importSummary) {
                result.warning = true;
                appendWorkflowDiagnostic(result.diagnostic, "rescan summary unavailable");
                return result;
            }
            std::vector<ImportManifestEntry> manifest;
            manifest.reserve(staging.files.size());
            for (const auto& file : staging.files) {
                const auto& fileResult = result.importSummary->files[file.summaryIndex];
                const bool hasValidRows = fileResult.status == ports::ImportFileStatus::Applied ||
                                          fileResult.status == ports::ImportFileStatus::NoChanges;
                manifest.push_back(
                    {file.consolidationSources, hasValidRows, file.consolidationFilename});
            }
            const auto consolidationPlan = stager_.planConsolidation(manifest, stopToken);
            if (consolidationPlan.canceled || !consolidationPlan.error.empty()) {
                result.warning = true;
                result.status = ports::WorkflowStatus::Succeeded;
                result.message += consolidationPlan.canceled ? " error=consolidation_canceled"
                                                             : " error=consolidation_failed";
                appendWorkflowDiagnostic(result.diagnostic, consolidationPlan.canceled
                                                                ? "rescan consolidation canceled"
                                                                : consolidationPlan.error);
                return result;
            }
            publishDatabaseSnapshot(workingDatabase, databasePath_, stopToken);
            const auto consolidation = stager_.consolidate(consolidationPlan, stopToken);
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
                auto& fileResult = result.importSummary->files[staging.files[index].summaryIndex];
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
                writer_.completeConsolidation(completedMoves);
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
                return canceled("rescan");
            }
            return {ports::WorkflowStatus::Failed, "rescan database snapshot failed", false,
                    error.what()};
        } catch (const ports::OperationError& error) {
            return {ports::WorkflowStatus::Failed, "rescan database snapshot failed", false,
                    error.diagnostic()};
        } catch (const std::exception& error) {
            return {ports::WorkflowStatus::Failed, "rescan database snapshot failed", false,
                    error.what()};
        }
    }

    ports::WorkflowResult
    SpreadsheetImportWorkflowPort::importIncrementalFiles(const ImportStagingResult& files,
                                                          const std::stop_token& stopToken) const {
        constexpr const char* operation = "import_xlsx_to_sqlite";
        if (files.files.size() <= 1 || files.operationalFailure || !files.rejectionReason.empty()) {
            return importDiscoveredFiles(files, false, stopToken);
        }

        auto summary = makeImportSummary(files);
        std::optional<ports::WorkflowResult> firstFailure;
        bool completedFile = false;
        bool changed = false;
        bool interrupted = summary.rejected > 0;
        bool warning = files.warning;
        std::string diagnostic = files.diagnostic;
        for (std::size_t fileIndex = 0; fileIndex < files.files.size(); ++fileIndex) {
            if (stopToken.stop_requested()) {
                interrupted = true;
                firstFailure = canceled(operation);
                for (; fileIndex < files.files.size(); ++fileIndex) {
                    auto& pending = summary.files[files.files[fileIndex].summaryIndex];
                    pending.status = ports::ImportFileStatus::Canceled;
                    ++summary.preserved;
                }
                break;
            }

            auto stagedFile = files.files[fileIndex];
            stagedFile.summaryIndex = 0;
            ImportStagingResult single;
            single.files.push_back(std::move(stagedFile));
            single.discoveredXlsxSources.push_back(
                files.discoveredXlsxSources[files.files[fileIndex].summaryIndex]);
            single.discovered = 1;
            auto result = importDiscoveredFiles(single, false, stopToken);
            if (!result.importSummary || result.importSummary->files.size() != 1) {
                result = {ports::WorkflowStatus::Failed, "import_xlsx_to_sqlite summary_failed",
                          false, "single-file import did not return one file result"};
            } else {
                const auto& singleSummary = *result.importSummary;
                summary.files[files.files[fileIndex].summaryIndex] = singleSummary.files.front();
                summary.accepted += singleSummary.accepted;
                summary.rejected += singleSummary.rejected;
                summary.preserved += singleSummary.preserved;
                summary.validRows += singleSummary.validRows;
                summary.invalidRows += singleSummary.invalidRows;
                summary.invalidNumberRows += singleSummary.invalidNumberRows;
                summary.invalidDescriptionRows += singleSummary.invalidDescriptionRows;
                summary.invalidDateRows += singleSummary.invalidDateRows;
                summary.skippedRows += singleSummary.skippedRows;
                summary.duplicateRows += singleSummary.duplicateRows;
                summary.inserts += singleSummary.inserts;
                summary.updates += singleSummary.updates;
                summary.unchangedRows += singleSummary.unchangedRows;
                summary.conflicts += singleSummary.conflicts;
                summary.consolidated += singleSummary.consolidated;
                summary.noSurvivor += singleSummary.noSurvivor;
            }
            appendWorkflowDiagnostic(diagnostic, result.diagnostic);
            warning = warning || result.warning;
            if (result.ok()) {
                completedFile = true;
                changed = changed || result.status == ports::WorkflowStatus::Succeeded;
                continue;
            }

            interrupted = true;
            if (!firstFailure) {
                firstFailure = result;
            }
            if (result.status != ports::WorkflowStatus::Canceled) {
                continue;
            }
            firstFailure = result;
            for (++fileIndex; fileIndex < files.files.size(); ++fileIndex) {
                auto& pending = summary.files[files.files[fileIndex].summaryIndex];
                pending.status = ports::ImportFileStatus::Canceled;
                ++summary.preserved;
            }
            break;
        }

        if (!completedFile) {
            auto result = firstFailure.value_or(ports::WorkflowResult{
                ports::WorkflowStatus::Failed, "import_xlsx_to_sqlite no_file_completed"});
            result.diagnostic = std::move(diagnostic);
            return withSummary(std::move(result), summary);
        }

        SsaImportWriteSummary total;
        total.files = files.files.size();
        total.rowsInserted = summary.inserts;
        total.rowsUpdated = summary.updates;
        total.rowsWritten = summary.inserts + summary.updates;
        total.rowsUnchanged = summary.unchangedRows;
        total.skippedRows = summary.skippedRows;
        total.duplicateRows = summary.duplicateRows;
        total.invalidRows = summary.invalidRows;
        total.invalidNumberRows = summary.invalidNumberRows;
        total.invalidDescriptionRows = summary.invalidDescriptionRows;
        total.invalidDateRows = summary.invalidDateRows;
        total.conflictRows = summary.conflicts;
        const auto error = interrupted ? std::string_view{"partial_failure"} : std::string_view{};
        return withSummary(
            {changed || interrupted ? ports::WorkflowStatus::Succeeded
                                    : ports::WorkflowStatus::NoChanges,
             workflowMessage(operation, files, total, {summary.rejected, error}),
             warning || interrupted || summary.invalidRows > 0 || summary.duplicateRows > 0,
             std::move(diagnostic)},
            summary);
    }

    ports::WorkflowResult SpreadsheetImportWorkflowPort::importDiscoveredFiles(
        const ImportStagingResult& files, const bool replaceAll, const std::stop_token& stopToken,
        const std::vector<ports::SamArtifact>* samArtifacts) const {
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
            return withSummary({ports::WorkflowStatus::Failed,
                                "import_xlsx_to_sqlite staging_cleanup_failed", false,
                                files.diagnostic},
                               importSummary);
        }
        if (files.operationalFailure) {
            return discardBeforeCommit({ports::WorkflowStatus::Failed,
                                        std::string{operation} + " " + files.rejectionReason});
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
        std::vector<PendingImportOutcome> pendingOutcomes;
        pendingOutcomes.reserve(files.files.size());
        for (std::size_t fileIndex = 0; fileIndex < files.files.size(); ++fileIndex) {
            const auto& file = files.files[fileIndex];
            auto& fileResult = importSummary.files[file.summaryIndex];
            if (stopToken.stop_requested()) {
                return discardBeforeCommit(rollbackSession(*writeSession, canceled(operation)));
            }
            SsaImportBatch batch;
            std::size_t validRows = 0;
            bool invalidFullBatch = false;
            bool duplicateConflict = false;
            std::string samRejectionReason;
            try {
                if (samArtifacts != nullptr) {
                    auto sheets = reader_.readSheets(file.workbookPath, stopToken);
                    for (auto& table : sheets) {
                        table.originalFilename = file.originalFilename;
                        table.sourceModifiedTimestamp = file.sourceModifiedTimestamp;
                        table.sourceCreatedTimestamp = file.sourceCreatedTimestamp;
                    }
                    auto adapted = SamSpreadsheetAdapter::adapt(
                        sheets, samArtifacts->at(file.summaryIndex), stopToken);
                    if (!adapted.ok()) {
                        samRejectionReason = std::move(adapted.rejectionReason);
                    } else {
                        batch = mapper_.map(adapted.table, stopToken);
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
                    bool mappedWorksheet = false;
                    bool ignoredWorksheet = false;
                    bool invalidTrailingWorksheet = false;
                    bool fatalMapping = false;
                    bool fileCountedByWriter = false;
                    std::vector<std::string> headerRow;
                    batch.sourcePath = file.workbookPath;
                    reader_.readSheetChunks(
                        file.workbookPath, kImportRowsPerChunk,
                        [&](SpreadsheetTable table, const bool firstInSheet, const bool) {
                            if (fatalMapping || invalidTrailingWorksheet || invalidFullBatch ||
                                duplicateConflict) {
                                return;
                            }
                            if (firstInSheet) {
                                ignoredWorksheet = false;
                                headerRow.clear();
                            } else if (ignoredWorksheet) {
                                batch.skippedRows += table.rows.size();
                                return;
                            } else {
                                table.headerRow = headerRow;
                            }
                            table.originalFilename = file.originalFilename;
                            table.sourceModifiedTimestamp = file.sourceModifiedTimestamp;
                            table.sourceCreatedTimestamp = file.sourceCreatedTimestamp;
                            auto worksheetBatch = mapper_.map(table, stopToken);
                            if (worksheetBatch.mappingStatus ==
                                SpreadsheetMappingStatus::HeaderNotRecognized) {
                                if (mappedWorksheet) {
                                    invalidTrailingWorksheet = true;
                                } else {
                                    ignoredWorksheet = true;
                                    batch.skippedRows += worksheetBatch.skippedRows;
                                }
                                return;
                            }
                            if (worksheetBatch.mappingStatus != SpreadsheetMappingStatus::Mapped) {
                                batch.mappingStatus = worksheetBatch.mappingStatus;
                                fatalMapping = true;
                                return;
                            }
                            if (firstInSheet) {
                                headerRow = worksheetBatch.headerRow;
                                batch.mappedColumns += worksheetBatch.mappedColumns;
                            }
                            mappedWorksheet = true;
                            batch.skippedRows += worksheetBatch.skippedRows;
                            batch.invalidRows += worksheetBatch.invalidRows;
                            batch.invalidNumberRows += worksheetBatch.invalidNumberRows;
                            batch.invalidDescriptionRows += worksheetBatch.invalidDescriptionRows;
                            batch.invalidDateRows += worksheetBatch.invalidDateRows;
                            validRows += worksheetBatch.rows.size();
                            if (replaceAll && worksheetBatch.invalidRows > 0) {
                                invalidFullBatch = true;
                                return;
                            }
                            if (worksheetBatch.rows.empty()) {
                                return;
                            }
                            const std::vector<SsaImportBatch> chunk{std::move(worksheetBatch)};
                            const auto resolved =
                                conflictResolver_.resolveBySsaNumberKeepingUnkeyedRows(chunk);
                            fileResult.conflicts += resolved.conflictRows;
                            importSummary.conflicts += resolved.conflictRows;
                            totalSummary.conflictRows += resolved.conflictRows;
                            if (resolved.conflictRows > 0) {
                                duplicateConflict = true;
                                return;
                            }
                            const auto batchWrite =
                                writeSession->write(resolved, fileCountedByWriter ? 0 : 1, 0);
                            fileCountedByWriter = true;
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
                        },
                        stopToken);
                    if (invalidTrailingWorksheet) {
                        batch.mappingStatus = SpreadsheetMappingStatus::HeaderNotRecognized;
                    } else if (!fatalMapping) {
                        batch.mappingStatus = mappedWorksheet
                                                  ? SpreadsheetMappingStatus::Mapped
                                                  : SpreadsheetMappingStatus::HeaderNotRecognized;
                    }
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
            if (!samRejectionReason.empty()) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession, {ports::WorkflowStatus::Rejected,
                                    std::string{operation} + " " + samRejectionReason}));
            }
            fileResult.validRows = validRows;
            fileResult.invalidRows = batch.invalidRows;
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
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     workflowMessage(operation, files, totalSummary, {0, "duplicate_conflict"})}));
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
            if (invalidFullBatch) {
                return discardBeforeCommit(rollbackSession(
                    *writeSession,
                    {ports::WorkflowStatus::Rejected,
                     workflowMessage(operation, files, totalSummary, {0, "invalid_rows"})}));
            }
            pendingOutcomes.push_back({&file, validRows > 0, file.summaryIndex});
            if (validRows == 0) {
                fileResult.status = ports::ImportFileStatus::NoValidRows;
                if (replaceAll) {
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
        if (!replaceAll && !pendingOutcomes.empty() &&
            std::ranges::none_of(pendingOutcomes, [](const PendingImportOutcome& outcome) {
                return outcome.hasValidRows;
            })) {
            return discardBeforeCommit(
                rollbackSession(*writeSession, {ports::WorkflowStatus::Rejected,
                                                std::string{operation} + " no_valid_rows"}));
        }
        std::vector<ImportManifestEntry> manifest;
        manifest.reserve(pendingOutcomes.size());
        for (const auto& outcome : pendingOutcomes) {
            manifest.push_back({outcome.file->consolidationSources, outcome.hasValidRows,
                                outcome.file->consolidationFilename});
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
            importSummary.skippedRows = totalSummary.skippedRows;
            importSummary.duplicateRows = totalSummary.duplicateRows;
            importSummary.invalidNumberRows = totalSummary.invalidNumberRows;
            importSummary.invalidDescriptionRows = totalSummary.invalidDescriptionRows;
            importSummary.invalidDateRows = totalSummary.invalidDateRows;
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
            return withSummary({status, workflowMessage(operation, files, totalSummary),
                                files.warning || files.failedCopies > 0 ||
                                    files.failedLegacyXls > 0 || totalSummary.invalidRows > 0 ||
                                    totalSummary.duplicateRows > 0,
                                files.diagnostic},
                               importSummary);
        }
        const auto consolidation = stager_.consolidate(consolidationPlan, stopToken);
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
            writer_.completeConsolidation(completedMoves);
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
