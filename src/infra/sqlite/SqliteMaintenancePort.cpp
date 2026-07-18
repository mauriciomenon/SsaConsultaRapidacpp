#include "infra/sqlite/SqliteMaintenancePort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"

#include <sqlite3.h>

#include <string>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(const std::string& message, const bool warning = false,
                                        std::string diagnostic = {}) {
            return {ports::WorkflowStatus::Succeeded, message, warning, std::move(diagnostic)};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Canceled, "sqlite maintenance canceled"};
        }

        ports::WorkflowResult failed(std::string diagnostic) {
            return {ports::WorkflowStatus::Failed, "sqlite maintenance failed", false,
                    std::move(diagnostic)};
        }

    } // namespace

    SqliteMaintenancePort::SqliteMaintenancePort(std::filesystem::path databasePath)
        : SqliteMaintenancePort(std::move(databasePath), []() noexcept {}) {}

    SqliteMaintenancePort::SqliteMaintenancePort(std::filesystem::path databasePath,
                                                 PostCommitHook postCommitHook)
        : databasePath_(std::move(databasePath)), postCommitHook_(std::move(postCommitHook)) {
        if (!postCommitHook_) {
            postCommitHook_ = []() noexcept {};
        }
    }

    ports::WorkflowResult SqliteMaintenancePort::resetDatabase(const std::stop_token stopToken) {
        return runCommittedMaintenance("DELETE FROM ssa_table", stopToken,
                                       "database reset completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::cleanData(const std::stop_token stopToken) {
        return runCommittedMaintenance(
            "DELETE FROM ssa_table WHERE TRIM(COALESCE(numero_ssa, '')) = ''", stopToken,
            "data cleanup completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::vacuumAnalyze(const std::stop_token stopToken) {
        return runCommittedMaintenance(nullptr, stopToken, "vacuum/analyze completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::runCommittedMaintenance(
        const char* mutationSql, const std::stop_token& stopToken, const char* successMessage) {
        try {
            const SqliteDatabaseWriteLock writeLock(databasePath_);
            if (!writeLock.acquired()) {
                return stopToken.stop_requested() ? canceled()
                                                  : failed(std::string{writeLock.diagnostic()});
            }
            SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
            SqliteBusyHandler busy(connection.handle(), stopToken);
            SqliteProgressHandler progress(connection.handle(), stopToken);
            SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());

            auto rollback = [&](ports::WorkflowResult result) {
                progress.disable();
                busy.disable();
                try {
                    transaction.rollback();
                } catch (const ports::OperationError& error) {
                    result.status = ports::WorkflowStatus::Failed;
                    result.message = "sqlite maintenance failed";
                    result.diagnostic += "; " + error.diagnostic();
                }
                return result;
            };

            if (mutationSql != nullptr) {
                if (auto result = executeMaintenanceSql(connection, mutationSql,
                                                        busy.cancellationObserved())) {
                    return rollback(std::move(*result));
                }
            }
            if (auto result =
                    executeMaintenanceSql(connection,
                                          "CREATE INDEX IF NOT EXISTS idx_ssa_table_derivada_de "
                                          "ON ssa_table (derivada_de)",
                                          busy.cancellationObserved())) {
                return rollback(std::move(*result));
            }
            if (stopToken.stop_requested()) {
                return rollback(canceled());
            }
            try {
                transaction.commit();
                postCommitHook_();
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return rollback(canceled());
                }
                return rollback(failed(error.what()));
            } catch (const ports::OperationError& error) {
                return rollback(failed(error.diagnostic()));
            }

            if (stopToken.stop_requested()) {
                return succeeded(std::string{successMessage} + "; optimization canceled", true);
            }
            if (auto result = runOptimizationTasks(connection, busy.cancellationObserved())) {
                const auto suffix = result->status == ports::WorkflowStatus::Canceled
                                        ? "; optimization canceled"
                                        : "; optimization failed";
                return succeeded(std::string{successMessage} + suffix, true,
                                 std::move(result->diagnostic));
            }
            return succeeded(successMessage);
        } catch (const std::system_error& error) {
            if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                return canceled();
            }
            return failed(error.what());
        } catch (const ports::OperationError& error) {
            return failed(error.diagnostic());
        } catch (const std::exception& error) {
            return failed(error.what());
        }
    }

    std::optional<ports::WorkflowResult>
    SqliteMaintenancePort::executeMaintenanceSql(SqliteConnection& connection, const char* sql,
                                                 const std::atomic_bool* busyCancellationObserved) {
        char* error = nullptr;
        const int execRc = sqlite3_exec(connection.handle(), sql, nullptr, nullptr, &error);
        const std::string message = error == nullptr ? std::string{} : std::string{error};
        if (error != nullptr) {
            sqlite3_free(error);
        }
        const bool busyCanceled = (execRc == SQLITE_BUSY || execRc == SQLITE_LOCKED) &&
                                  busyCancellationObserved != nullptr &&
                                  busyCancellationObserved->load(std::memory_order_relaxed);
        if (execRc == SQLITE_INTERRUPT || busyCanceled) {
            return canceled();
        }
        if (execRc != SQLITE_OK) {
            return failed(
                "sqlite maintenance failed: rc=" + std::to_string(execRc) +
                " extended_rc=" + std::to_string(sqlite3_extended_errcode(connection.handle())) +
                " message=" + message);
        }
        return std::nullopt;
    }

    std::optional<ports::WorkflowResult>
    SqliteMaintenancePort::runOptimizationTasks(SqliteConnection& connection,
                                                const std::atomic_bool* busyCancellationObserved) {
        if (auto result = executeMaintenanceSql(connection, "VACUUM", busyCancellationObserved)) {
            return result;
        }
        return executeMaintenanceSql(connection, "ANALYZE", busyCancellationObserved);
    }

} // namespace ssa::infra::sqlite
