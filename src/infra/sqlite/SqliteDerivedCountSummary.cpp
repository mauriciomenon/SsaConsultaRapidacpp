#include "infra/sqlite/SqliteDerivedCountSummary.h"

#include "domain/SsaTypes.h"
#include "infra/sqlite/SqliteConnection.h"
#include "ports/OperationError.h"
#include "query/SqlQueryText.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string_view>
#include <system_error>

namespace ssa::infra::sqlite {

    namespace {

        std::string summaryTableName(const std::string& tableName) {
            return tableName + "_derived_counts";
        }

        std::string summaryMetaTableName(const std::string& tableName) {
            return tableName + "_derived_counts_meta";
        }

        void executeSql(sqlite3* db, const std::string& sql,
                        const std::atomic_bool* busyCancellationObserved) {
            char* error = nullptr;
            const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
            const bool busyCanceled = (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) &&
                                      busyCancellationObserved != nullptr &&
                                      busyCancellationObserved->load(std::memory_order_relaxed);
            if (rc == SQLITE_INTERRUPT || busyCanceled) {
                sqlite3_free(error);
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "sqlite derived count summary canceled");
            }
            if (rc != SQLITE_OK) {
                const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
                sqlite3_free(error);
                throw ports::OperationError(
                    "Falha ao atualizar resumo de derivadas",
                    "sqlite derived count summary failed: rc=" + std::to_string(rc) +
                        " extended_rc=" + std::to_string(sqlite3_extended_errcode(db)) +
                        " message=" + message);
            }
            sqlite3_free(error);
        }

        bool supportsSummary(const std::vector<domain::ColumnDef>& columns) {
            bool hasNumber = false;
            bool hasDerivation = false;
            for (const auto& column : columns) {
                hasNumber = hasNumber || column.key == domain::kSsaNumberColumnKey;
                hasDerivation = hasDerivation || column.key == "derivada_de";
            }
            return hasNumber && hasDerivation;
        }

    } // namespace

    void ensureDerivedCountSummary(sqlite3* db, const std::string& tableName,
                                   const std::vector<domain::ColumnDef>& columns,
                                   const std::atomic_bool* busyCancellationObserved) {
        if (!supportsSummary(columns)) {
            return;
        }
        const auto table = query::quoteTableIdentifier(tableName);
        const auto summaryTable = query::quoteTableIdentifier(summaryTableName(tableName));
        const auto metaTable = query::quoteTableIdentifier(summaryMetaTableName(tableName));
        const auto derivationColumn = query::quoteColumnIdentifier("derivada_de");
        const std::string parentColumn = "\"parent_ssa\"";
        const auto countColumn = query::quoteColumnIdentifier(
            std::string{domain::ColumnCatalog::derivedCountColumnKey()});
        const auto normalizedDerivation = "TRIM(COALESCE(" + derivationColumn + ", ''))";

        executeSql(db,
                   "CREATE TABLE IF NOT EXISTS " + summaryTable + " (" + parentColumn +
                       " TEXT PRIMARY KEY NOT NULL, " + countColumn + " INTEGER NOT NULL)",
                   busyCancellationObserved);
        executeSql(db,
                   "CREATE TABLE IF NOT EXISTS " + metaTable +
                       " (initialized INTEGER PRIMARY KEY NOT NULL CHECK(initialized = 1))",
                   busyCancellationObserved);

        SqliteStatement initializedQuery(db, "SELECT COUNT(*) FROM " + metaTable,
                                         busyCancellationObserved);
        const bool initialized = initializedQuery.step() && initializedQuery.columnInt64(0) != 0;
        if (!initialized) {
            executeSql(db, "DELETE FROM " + summaryTable, busyCancellationObserved);
            executeSql(db,
                       "INSERT INTO " + summaryTable + " (" + parentColumn + ", " + countColumn +
                           ") SELECT " + normalizedDerivation + ", COUNT(*) FROM " + table +
                           " WHERE " + normalizedDerivation + " <> '' GROUP BY " +
                           normalizedDerivation,
                       busyCancellationObserved);
            executeSql(db, "INSERT INTO " + metaTable + " (initialized) VALUES (1)",
                       busyCancellationObserved);
        }

        const auto addParent = [&](const std::string& value) {
            const auto normalizedValue = "TRIM(COALESCE(" + value + ", ''))";
            return "INSERT INTO " + summaryTable + " (" + parentColumn + ", " + countColumn +
                   ") SELECT " + normalizedValue + ", 1 WHERE " + normalizedValue +
                   " <> '' ON CONFLICT(" + parentColumn + ") DO UPDATE SET " + countColumn + " = " +
                   countColumn + " + 1";
        };
        const auto removeParent = [&](const std::string& value) {
            const auto normalizedValue = "TRIM(COALESCE(" + value + ", ''))";
            return "UPDATE " + summaryTable + " SET " + countColumn + " = " + countColumn +
                   " - 1 WHERE " + parentColumn + " = " + normalizedValue + "; DELETE FROM " +
                   summaryTable + " WHERE " + parentColumn + " = " + normalizedValue + " AND " +
                   countColumn + " <= 0";
        };
        const auto oldParent = "OLD." + derivationColumn;
        const auto newParent = "NEW." + derivationColumn;
        const auto triggerPrefix = "trg_" + tableName + "_derived_count_";
        executeSql(db,
                   "CREATE TRIGGER IF NOT EXISTS " +
                       query::quoteTableIdentifier(triggerPrefix + "insert") + " AFTER INSERT ON " +
                       table + " WHEN TRIM(COALESCE(" + newParent + ", '')) <> '' BEGIN " +
                       addParent(newParent) + "; END",
                   busyCancellationObserved);
        executeSql(db,
                   "CREATE TRIGGER IF NOT EXISTS " +
                       query::quoteTableIdentifier(triggerPrefix + "delete") + " AFTER DELETE ON " +
                       table + " WHEN TRIM(COALESCE(" + oldParent + ", '')) <> '' BEGIN " +
                       removeParent(oldParent) + "; END",
                   busyCancellationObserved);
        executeSql(db,
                   "CREATE TRIGGER IF NOT EXISTS " +
                       query::quoteTableIdentifier(triggerPrefix + "update") + " AFTER UPDATE OF " +
                       derivationColumn + " ON " + table + " WHEN TRIM(COALESCE(" + oldParent +
                       ", '')) <> TRIM(COALESCE(" + newParent + ", '')) BEGIN " +
                       removeParent(oldParent) + "; " + addParent(newParent) + "; END",
                   busyCancellationObserved);
    }

} // namespace ssa::infra::sqlite
