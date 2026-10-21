#pragma once

#include "infra/sqlite/SqliteProgressHandler.h"

#include <chrono>
#include <filesystem>
#include <stop_token>

namespace ssa::infra::sqlite {

    struct SqliteBackupMessages {
        const char* safeMessage; // user-facing message of the OperationError
        const char* operation;   // diagnostic prefix
        const char* initOperation;
    };

    // Online snapshot of `source` into `destination` with the SQLite backup API, so committed
    // pages still in a WAL file are included. Throws ports::OperationError on failure and
    // std::system_error(operation_canceled) when the stop token fires. `lockedSignal`, when
    // given, is released once the first SQLITE_LOCKED step is observed.
    void backupSqliteDatabase(const std::filesystem::path& source,
                              const std::filesystem::path& destination,
                              const std::stop_token& stopToken, std::chrono::milliseconds busyWait,
                              const SqliteBackupMessages& messages,
                              SqliteSynchronizationSemaphore* lockedSignal = nullptr);

} // namespace ssa::infra::sqlite
