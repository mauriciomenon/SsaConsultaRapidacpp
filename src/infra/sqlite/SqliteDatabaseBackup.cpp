#include "infra/sqlite/SqliteDatabaseBackup.h"

#include "infra/sqlite/SqliteConnection.h"
#include "ports/IWorkflowPorts.h"
#include "ports/OperationError.h"

#include <sqlite3.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <system_error>
#include <thread>

namespace ssa::infra::sqlite {

    namespace {
        constexpr int kPagesPerStep = 256;

        std::string backupError(sqlite3* database, const char* operation, const int rc) {
            return std::string{operation} + " failed: rc=" + std::to_string(rc) +
                   " message=" + sqlite3_errmsg(database);
        }
    } // namespace

    // Call sites name source and destination explicitly.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void backupSqliteDatabase(const std::filesystem::path& source,
                              const std::filesystem::path& destination,
                              const std::stop_token& stopToken,
                              const std::chrono::milliseconds busyWait,
                              const SqliteBackupMessages& messages,
                              SqliteSynchronizationSemaphore* lockedSignal) {
        SqliteConnection sourceConnection(source, SqliteOpenMode::ReadOnly,
                                          std::chrono::milliseconds{0});
        SqliteConnection destinationConnection(destination, SqliteOpenMode::ReadWriteCreate,
                                               std::chrono::milliseconds{0});
        std::atomic_flag lockReported = ATOMIC_FLAG_INIT;
        SqliteBusyHandler busyHandler(destinationConnection.handle(), stopToken, busyWait,
                                      lockedSignal,
                                      lockedSignal != nullptr ? &lockReported : nullptr);
        auto* backup = sqlite3_backup_init(destinationConnection.handle(), "main",
                                           sourceConnection.handle(), "main");
        if (backup == nullptr) {
            throw ports::OperationError(
                messages.safeMessage,
                backupError(destinationConnection.handle(), messages.initOperation,
                            sqlite3_errcode(destinationConnection.handle())));
        }
        int stepResult = SQLITE_OK;
        auto lockedRetryDeadline = std::chrono::steady_clock::time_point{};
        while (stepResult == SQLITE_OK || stepResult == SQLITE_LOCKED) {
            if (stopToken.stop_requested()) {
                stepResult = SQLITE_INTERRUPT;
                break;
            }
            stepResult = sqlite3_backup_step(backup, kPagesPerStep);
            if (stepResult == SQLITE_LOCKED) {
                if (lockedSignal != nullptr &&
                    !lockReported.test_and_set(std::memory_order_relaxed)) {
                    lockedSignal->release();
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
                               now + ports::ImportExecutionOptions::kSqliteBusyRetryGranularity));
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
                messages.safeMessage,
                backupError(destinationConnection.handle(), messages.operation, errorCode));
        }
    }

} // namespace ssa::infra::sqlite
