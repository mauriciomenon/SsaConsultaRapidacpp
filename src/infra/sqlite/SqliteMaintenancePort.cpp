#include "infra/sqlite/SqliteMaintenancePort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"

#include <sqlite3.h>

#include <string>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(const char* operation) {
            return {ports::WorkflowStatus::Succeeded, operation};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Canceled, "sqlite maintenance canceled"};
        }

    } // namespace

    SqliteMaintenancePort::SqliteMaintenancePort(std::filesystem::path databasePath)
        : databasePath_(std::move(databasePath)) {}

    ports::WorkflowResult SqliteMaintenancePort::resetDatabase(const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled();
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        if (auto result = executeMaintenanceSql(connection, "DELETE FROM ssa_table")) {
            return *result;
        }
        if (auto result = runOptimizationTasks(connection)) {
            return *result;
        }
        return succeeded("database reset completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::cleanData(const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled();
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        if (auto result = executeMaintenanceSql(
                connection, "DELETE FROM ssa_table WHERE TRIM(COALESCE(numero_ssa, '')) = ''")) {
            return *result;
        }
        if (auto result = runOptimizationTasks(connection)) {
            return *result;
        }
        return succeeded("data cleanup completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::vacuumAnalyze(const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled();
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        if (auto result = runOptimizationTasks(connection)) {
            return *result;
        }
        return succeeded("vacuum/analyze completed");
    }

    std::optional<ports::WorkflowResult>
    SqliteMaintenancePort::executeMaintenanceSql(SqliteConnection& connection, const char* sql) {
        char* error = nullptr;
        const int execRc = sqlite3_exec(connection.handle(), sql, nullptr, nullptr, &error);
        const std::string message = error == nullptr ? std::string{} : std::string{error};
        if (error != nullptr) {
            sqlite3_free(error);
        }
        if (execRc == SQLITE_INTERRUPT) {
            return canceled();
        }
        if (execRc != SQLITE_OK) {
            return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                         "sqlite maintenance failed: " + message};
        }
        return std::nullopt;
    }

    std::optional<ports::WorkflowResult>
    SqliteMaintenancePort::runOptimizationTasks(SqliteConnection& connection) {
        if (auto result = executeMaintenanceSql(
                connection, "CREATE INDEX IF NOT EXISTS idx_ssa_table_derivada_de "
                            "ON ssa_table (derivada_de)")) {
            return result;
        }
        if (auto result = executeMaintenanceSql(connection, "VACUUM")) {
            return result;
        }
        return executeMaintenanceSql(connection, "ANALYZE");
    }

} // namespace ssa::infra::sqlite
