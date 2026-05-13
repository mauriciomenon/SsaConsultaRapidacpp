#include "infra/sqlite/SqliteMaintenancePort.h"

#include "infra/sqlite/SqliteConnection.h"

#include <sqlite3.h>

#include <string>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(const char* operation) {
            return {ports::WorkflowStatus::Succeeded, operation};
        }

    } // namespace

    SqliteMaintenancePort::SqliteMaintenancePort(std::filesystem::path databasePath)
        : databasePath_(std::move(databasePath)) {}

    ports::WorkflowResult SqliteMaintenancePort::resetDatabase() {
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        if (auto result = executeMaintenanceSql(connection, "DELETE FROM ssa_table")) {
            return *result;
        }
        if (auto result = runOptimizationTasks(connection)) {
            return *result;
        }
        return succeeded("database reset completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::cleanData() {
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        if (auto result = executeMaintenanceSql(
                connection, "DELETE FROM ssa_table WHERE TRIM(COALESCE(numero_ssa, '')) = ''")) {
            return *result;
        }
        if (auto result = runOptimizationTasks(connection)) {
            return *result;
        }
        return succeeded("data cleanup completed");
    }

    ports::WorkflowResult SqliteMaintenancePort::vacuumAnalyze() {
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
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
        if (execRc != SQLITE_OK) {
            return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                         "sqlite maintenance failed: " + message};
        }
        return std::nullopt;
    }

    std::optional<ports::WorkflowResult>
    SqliteMaintenancePort::runOptimizationTasks(SqliteConnection& connection) {
        if (auto result = executeMaintenanceSql(connection, "VACUUM")) {
            return result;
        }
        return executeMaintenanceSql(connection, "ANALYZE");
    }

} // namespace ssa::infra::sqlite
