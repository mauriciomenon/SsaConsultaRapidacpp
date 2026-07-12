#include "infra/sqlite/SqliteDerivadasPort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"

#include <sqlite3.h>

#include <optional>
#include <string>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(std::size_t fixedRecords) {
            return {ports::WorkflowStatus::Succeeded,
                    "derivadas sync completed; " + std::to_string(fixedRecords) + " records fixed"};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Rejected, "sqlite derivadas sync canceled"};
        }

        std::optional<ports::WorkflowResult> executeSyncSql(SqliteConnection& connection,
                                                            const char* sql) {
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
                                             "sqlite derivadas sync failed: " + message};
            }
            return std::nullopt;
        }

    } // namespace

    SqliteDerivadasPort::SqliteDerivadasPort(std::filesystem::path databasePath)
        : databasePath_(std::move(databasePath)) {}

    ports::WorkflowResult SqliteDerivadasPort::syncDerivadas(const std::stop_token stopToken) {
        if (stopToken.stop_requested()) {
            return canceled();
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        // Run inside an explicit immediate transaction: the original auto-commit
        // form issued the UPDATE as one implicit transaction per statement, and
        // the correlated NOT EXISTS subquery did a full table scan with an index
        // probe per non-blank row. Wrapping in BEGIN IMMEDIATE also serializes
        // the write cleanly.
        if (auto result = executeSyncSql(connection, "BEGIN IMMEDIATE")) {
            return *result;
        }
        // Materialize the orphan derivada_de values once (using the numero_ssa
        // index for the anti-join) instead of re-evaluating a correlated NOT
        // EXISTS per row. The CTE resolves the set of broken references first,
        // then the UPDATE joins against it - a single scan of the index plus one
        // pass over candidates.
        constexpr const char* operationSql =
            "WITH orphan_refs AS (\n"
            "    SELECT DISTINCT TRIM(derivada_de) AS orphan\n"
            "    FROM ssa_table\n"
            "    WHERE TRIM(COALESCE(derivada_de, '')) <> ''\n"
            "      AND NOT EXISTS (\n"
            "          SELECT 1 FROM ssa_table AS parents\n"
            "          WHERE parents.numero_ssa = TRIM(ssa_table.derivada_de)\n"
            "      )\n"
            ")\n"
            "UPDATE ssa_table\n"
            "SET derivada_de = NULL\n"
            "WHERE TRIM(COALESCE(derivada_de, '')) IN (SELECT orphan FROM orphan_refs)";
        if (auto result = executeSyncSql(connection, operationSql)) {
            executeSyncSql(connection, "ROLLBACK");
            return *result;
        }
        const auto fixedRecords = static_cast<std::size_t>(sqlite3_changes(connection.handle()));
        if (auto result = executeSyncSql(connection, "COMMIT")) {
            return *result;
        }
        return succeeded(fixedRecords);
    }

} // namespace ssa::infra::sqlite
