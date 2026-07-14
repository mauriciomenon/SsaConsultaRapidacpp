#include "infra/sqlite/SqliteDerivadasPort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"

#include <sqlite3.h>

#include <optional>
#include <string>

namespace ssa::infra::sqlite {

    namespace {

        ports::WorkflowResult succeeded(std::size_t fixedRecords) {
            return {ports::WorkflowStatus::Succeeded, "orphan derivation cleanup completed; " +
                                                          std::to_string(fixedRecords) +
                                                          " records fixed"};
        }

        ports::WorkflowResult canceled() {
            return {ports::WorkflowStatus::Canceled, "sqlite orphan derivation cleanup canceled"};
        }

        ports::WorkflowResult failed(std::string diagnostic) {
            return {ports::WorkflowStatus::Failed, "sqlite orphan derivation cleanup failed", false,
                    std::move(diagnostic)};
        }

        std::optional<ports::WorkflowResult>
        executeSyncSql(SqliteConnection& connection, const char* sql,
                       const std::atomic_bool* canceledByBusy) {
            char* error = nullptr;
            const int execRc = sqlite3_exec(connection.handle(), sql, nullptr, nullptr, &error);
            const std::string message = error == nullptr ? std::string{} : std::string{error};
            if (error != nullptr) {
                sqlite3_free(error);
            }
            const bool busyCanceled = (execRc == SQLITE_BUSY || execRc == SQLITE_LOCKED) &&
                                      canceledByBusy != nullptr &&
                                      canceledByBusy->load(std::memory_order_relaxed);
            if (execRc == SQLITE_INTERRUPT || busyCanceled) {
                return canceled();
            }
            if (execRc != SQLITE_OK) {
                return failed("sqlite orphan derivation cleanup failed: rc=" +
                              std::to_string(execRc) + " extended_rc=" +
                              std::to_string(sqlite3_extended_errcode(connection.handle())) +
                              " message=" + message);
            }
            return std::nullopt;
        }

    } // namespace

    SqliteDerivadasPort::SqliteDerivadasPort(std::filesystem::path databasePath)
        : databasePath_(std::move(databasePath)) {}

    ports::WorkflowResult
    SqliteDerivadasPort::cleanOrphanDerivations(const std::stop_token stopToken) {
        try {
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
                    result.message = "sqlite orphan derivation cleanup failed";
                    result.diagnostic += "; " + error.diagnostic();
                }
                return result;
            };

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
            if (auto result =
                    executeSyncSql(connection, operationSql, busy.cancellationObserved())) {
                return rollback(std::move(*result));
            }
            const auto fixedRecords =
                static_cast<std::size_t>(sqlite3_changes(connection.handle()));
            if (stopToken.stop_requested()) {
                return rollback(canceled());
            }
            try {
                transaction.commit();
            } catch (const std::system_error& error) {
                if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                    return rollback(canceled());
                }
                return rollback(failed(error.what()));
            } catch (const ports::OperationError& error) {
                return rollback(failed(error.diagnostic()));
            }
            return succeeded(fixedRecords);
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

} // namespace ssa::infra::sqlite
