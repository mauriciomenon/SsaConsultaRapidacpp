#include "infra/sqlite/SqliteDerivadasPort.h"

#include "infra/sqlite/SqliteConnection.h"

#include <sqlite3.h>

#include <optional>
#include <string>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(std::size_t fixedRecords) {
            return {ports::WorkflowStatus::Succeeded,
                    "derivadas sync completed; " + std::to_string(fixedRecords) + " records fixed"};
        }

        std::optional<ports::WorkflowResult> executeSyncSql(SqliteConnection& connection,
                                                            const char* sql) {
            char* error = nullptr;
            const int execRc = sqlite3_exec(connection.handle(), sql, nullptr, nullptr, &error);
            const std::string message = error == nullptr ? std::string{} : std::string{error};
            if (error != nullptr) {
                sqlite3_free(error);
            }
            if (execRc != SQLITE_OK) {
                return ports::WorkflowResult{ports::WorkflowStatus::Failed,
                                             "sqlite derivadas sync failed: " + message};
            }
            return std::nullopt;
        }

    } // namespace

    SqliteDerivadasPort::SqliteDerivadasPort(std::filesystem::path databasePath)
        : databasePath_(std::move(databasePath)) {}

    ports::WorkflowResult SqliteDerivadasPort::syncDerivadas() {
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        constexpr const char* operationSql =
            "UPDATE ssa_table\n"
            "SET derivada_de = NULL\n"
            "WHERE TRIM(COALESCE(derivada_de, '')) <> ''\n"
            "  AND NOT EXISTS (\n"
            "      SELECT 1\n"
            "      FROM ssa_table AS parents\n"
            "      WHERE parents.numero_ssa = ssa_table.derivada_de\n"
            "  )";
        if (auto result = executeSyncSql(connection, operationSql)) {
            return *result;
        }
        return succeeded(static_cast<std::size_t>(sqlite3_changes(connection.handle())));
    }

} // namespace ssa::infra::sqlite
