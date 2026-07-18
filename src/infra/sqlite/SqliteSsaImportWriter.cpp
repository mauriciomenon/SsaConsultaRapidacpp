#include "infra/sqlite/SqliteSsaImportWriter.h"

#include "domain/SsaImportPolicy.h"
#include "domain/SsaTypes.h"
#include "domain/WhitespaceTrim.h"
#include "infra/import/CancelableFileCopy.h"
#include "infra/sqlite/SqliteActivityAnalyticsProjection.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivedCountSummary.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"
#include "query/SqlQueryText.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ssa::infra::sqlite {

    namespace {

        constexpr std::string_view kConsolidationJournalTable = "ssa_import_consolidation_journal";

        void executeSql(sqlite3* db, const std::string& sql,
                        const std::atomic_bool* busyCancellationObserved = nullptr) {
            char* error = nullptr;
            const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
            const bool busyCanceled = (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) &&
                                      busyCancellationObserved != nullptr &&
                                      busyCancellationObserved->load(std::memory_order_relaxed);
            if (rc == SQLITE_INTERRUPT || busyCanceled) {
                sqlite3_free(error);
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "sqlite import canceled");
            }
            if (rc != SQLITE_OK) {
                const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
                sqlite3_free(error);
                throw ports::OperationError(
                    "Falha ao atualizar o banco de dados",
                    "sqlite import command failed: rc=" + std::to_string(rc) + " extended_rc=" +
                        std::to_string(sqlite3_extended_errcode(db)) + " message=" + message);
            }
            sqlite3_free(error);
        }

        bool usesLegacySchemaVersion(sqlite3* db,
                                     const std::atomic_bool* busyCancellationObserved = nullptr) {
            SqliteStatement version(db, "PRAGMA user_version", busyCancellationObserved);
            if (!version.step()) {
                throw ports::OperationError("Falha ao validar versao do schema SQLite",
                                            "sqlite user_version query returned no row");
            }
            const auto actualVersion = version.columnInt64(0);
            const auto currentVersion = domain::ColumnCatalog::schemaVersion();
            if (actualVersion != 0 && actualVersion != currentVersion) {
                throw ports::OperationError(
                    "Falha ao validar versao do schema SQLite",
                    "unsupported sqlite user_version=" + std::to_string(actualVersion) +
                        " current=" + std::to_string(currentVersion));
            }
            return actualVersion == 0;
        }

        std::string createConsolidationJournalSql() {
            return "CREATE TABLE IF NOT EXISTS " + std::string{kConsolidationJournalTable} +
                   " (source TEXT PRIMARY KEY NOT NULL, destination TEXT NOT NULL, "
                   "has_valid_rows INTEGER NOT NULL CHECK(has_valid_rows IN (0, 1)), "
                   "source_identity TEXT, source_size INTEGER)";
        }

        bool
        consolidationJournalHasColumn(sqlite3* db, const std::string_view column,
                                      const std::atomic_bool* busyCancellationObserved = nullptr) {
            SqliteStatement columns(db, "PRAGMA table_info(ssa_import_consolidation_journal)",
                                    busyCancellationObserved);
            while (columns.step()) {
                if (columns.columnText(1) == column) {
                    return true;
                }
            }
            return false;
        }

        void ensureConsolidationJournalSchema(
            sqlite3* db, const std::atomic_bool* busyCancellationObserved = nullptr) {
            executeSql(db, createConsolidationJournalSql(), busyCancellationObserved);
            if (!consolidationJournalHasColumn(db, "source_identity", busyCancellationObserved)) {
                executeSql(db,
                           "ALTER TABLE ssa_import_consolidation_journal "
                           "ADD COLUMN source_identity TEXT",
                           busyCancellationObserved);
            }
            if (!consolidationJournalHasColumn(db, "source_size", busyCancellationObserved)) {
                executeSql(db,
                           "ALTER TABLE ssa_import_consolidation_journal "
                           "ADD COLUMN source_size INTEGER",
                           busyCancellationObserved);
            }
        }

        std::filesystem::path journalPath(const std::string& value) {
            return std::filesystem::absolute(
                       qt::toFileSystemPath(
                           QString::fromUtf8(value.c_str(), static_cast<qsizetype>(value.size()))))
                .lexically_normal();
        }

        bool isValidSqlIdentifier(const std::string_view value) {
            if (value.empty()) {
                return false;
            }
            const auto first = static_cast<unsigned char>(value.front());
            if (std::isalpha(first) == 0 && value.front() != '_') {
                return false;
            }
            return std::ranges::all_of(value, [](const char ch) {
                const auto byte = static_cast<unsigned char>(ch);
                return std::isalnum(byte) != 0 || ch == '_';
            });
        }

        bool isIdColumnKey(const std::string& key) {
            return key.size() == 2 && std::tolower(static_cast<unsigned char>(key[0])) == 'i' &&
                   std::tolower(static_cast<unsigned char>(key[1])) == 'd';
        }

        std::string createTableSql(const std::string& tableName,
                                   const std::vector<domain::ColumnDef>& columns) {
            std::ostringstream sql;
            sql << "CREATE TABLE IF NOT EXISTS " << query::quoteTableIdentifier(tableName) << " (";
            bool hasIdColumn = false;
            bool hasAnyColumn = false;
            for (const auto& column : columns) {
                if (hasAnyColumn) {
                    sql << ", ";
                }
                if (isIdColumnKey(column.key)) {
                    sql << query::quoteColumnIdentifier(column.key) << " INTEGER PRIMARY KEY";
                    hasIdColumn = true;
                } else {
                    sql << query::quoteColumnIdentifier(column.key) << " "
                        << (column.type == domain::ColumnType::Integer ? "INTEGER" : "TEXT");
                }
                hasAnyColumn = true;
            }
            if (!hasIdColumn) {
                if (hasAnyColumn) {
                    sql << ", ";
                }
                sql << "id INTEGER PRIMARY KEY";
            }
            sql << ")";
            return sql.str();
        }

        std::string legacySsaNumberIndexName(const std::string& tableName) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "idx_" + tableName + "_" + ssaNumberColumn;
        }

        bool isSsaReferenceColumn(std::string_view key);

        std::string createUniqueSsaNumberIndexSql(const std::string& tableName) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "CREATE UNIQUE INDEX " +
                   query::quoteTableIdentifier("ux_" + tableName + "_" + ssaNumberColumn) + " ON " +
                   query::quoteTableIdentifier(tableName) + " (" +
                   query::quoteColumnIdentifier(ssaNumberColumn) + ")";
        }

        std::string dirtyCanonicalLedgerIndexName(const std::string& tableName) {
            return "idx_" + tableName + "_import_dirty_canonical";
        }

        std::string dirtyCanonicalTerm(const std::string_view columnName, const bool isIdentity) {
            const auto column = query::quoteColumnIdentifier(std::string{columnName});
            const auto embeddedNul = "INSTR(" + column + ", CHAR(0)) <> 0";
            const auto notExactNumber =
                column + " NOT GLOB '[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'";
            if (isIdentity) {
                return "(" + column + " IS NULL OR TYPEOF(" + column + ") <> 'text' OR " +
                       embeddedNul + " OR " + notExactNumber + ")";
            }
            return "(" + column + " IS NOT NULL AND (TYPEOF(" + column + ") <> 'text' OR " +
                   embeddedNul + " OR (" + column + " <> '' AND " + notExactNumber + ")))";
        }

        std::vector<std::string>
        ssaReferenceColumns(const std::vector<domain::ColumnDef>& columns) {
            std::vector<std::string> references;
            for (const auto& column : columns) {
                if (isSsaReferenceColumn(column.key)) {
                    references.push_back(column.key);
                }
            }
            std::ranges::sort(references);
            return references;
        }

        std::string dirtyCanonicalPredicate(const std::vector<domain::ColumnDef>& columns) {
            std::string predicate = dirtyCanonicalTerm(domain::kSsaNumberColumnKey, true);
            for (const auto& reference : ssaReferenceColumns(columns)) {
                predicate += " OR " + dirtyCanonicalTerm(reference, false);
            }
            return predicate;
        }

        std::string createDirtyCanonicalLedgerSql(const std::string& tableName,
                                                  const std::string& predicate) {
            return "CREATE INDEX " +
                   query::quoteTableIdentifier(dirtyCanonicalLedgerIndexName(tableName)) + " ON " +
                   query::quoteTableIdentifier(tableName) + " (" +
                   query::quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) +
                   ") WHERE " + predicate;
        }

        bool hasExactIndexSql(sqlite3* db, const std::string& tableName,
                              const std::string& indexName, const std::string& expectedSql,
                              const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(
                db, "SELECT sql FROM sqlite_master WHERE type='index' AND name=? AND tbl_name=?",
                busyCancellationObserved);
            statement.bindTextOneBased(1, indexName);
            statement.bindTextOneBased(2, tableName);
            return statement.step() && statement.columnText(0) == expectedSql;
        }

        bool hasNamedIndex(sqlite3* db, const std::string& indexName,
                           const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(db,
                                      "SELECT 1 FROM sqlite_master WHERE type='index' AND name=?",
                                      busyCancellationObserved);
            statement.bindTextOneBased(1, indexName);
            return statement.step();
        }

        void ensureExactIndex(sqlite3* db, const std::string& tableName,
                              const std::string& indexName, const std::string& createSql,
                              const std::atomic_bool* busyCancellationObserved) {
            if (hasExactIndexSql(db, tableName, indexName, createSql, busyCancellationObserved)) {
                return;
            }
            if (hasNamedIndex(db, indexName, busyCancellationObserved)) {
                executeSql(db, "DROP INDEX " + query::quoteTableIdentifier(indexName),
                           busyCancellationObserved);
            }
            executeSql(db, createSql, busyCancellationObserved);
        }

        bool dirtyCanonicalLedgerHasRows(sqlite3* db, const std::string& tableName,
                                         const std::string& predicate,
                                         const std::atomic_bool* busyCancellationObserved) {
            const auto indexName = dirtyCanonicalLedgerIndexName(tableName);
            const auto sql = "SELECT 1 FROM " + query::quoteTableIdentifier(tableName) +
                             " INDEXED BY " + query::quoteTableIdentifier(indexName) + " WHERE " +
                             predicate + " LIMIT 1";
            SqliteStatement statement(db, sql, busyCancellationObserved);
            return statement.step();
        }

        std::vector<std::string> legacyDirtyCanonicalIndexNames(const std::string& tableName) {
            std::vector<std::string> columns{std::string{domain::kSsaNumberColumnKey}};
            for (const auto& column : domain::ColumnCatalog::all()) {
                if (isSsaReferenceColumn(column.key)) {
                    columns.push_back(column.key);
                }
            }
            std::ranges::sort(columns);
            columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
            std::vector<std::string> names;
            names.reserve(columns.size());
            for (const auto& column : columns) {
                names.push_back("idx_" + tableName + "_import_dirty_" + column);
            }
            return names;
        }

        void dropLegacyDirtyCanonicalIndexes(sqlite3* db, const std::string& tableName,
                                             const std::atomic_bool* busyCancellationObserved) {
            for (const auto& indexName : legacyDirtyCanonicalIndexNames(tableName)) {
                if (hasNamedIndex(db, indexName, busyCancellationObserved)) {
                    executeSql(db, "DROP INDEX " + query::quoteTableIdentifier(indexName),
                               busyCancellationObserved);
                }
            }
        }

        void dropLegacySsaNumberIndex(sqlite3* db, const std::string& tableName,
                                      const std::atomic_bool* busyCancellationObserved) {
            const auto indexName = legacySsaNumberIndexName(tableName);
            if (hasNamedIndex(db, indexName, busyCancellationObserved)) {
                executeSql(db, "DROP INDEX " + query::quoteTableIdentifier(indexName),
                           busyCancellationObserved);
            }
        }

        void ensureDirtyCanonicalLedger(sqlite3* db, const std::string& tableName,
                                        const std::vector<domain::ColumnDef>& columns,
                                        const std::atomic_bool* busyCancellationObserved) {
            const auto predicate = dirtyCanonicalPredicate(columns);
            const auto indexName = dirtyCanonicalLedgerIndexName(tableName);
            ensureExactIndex(db, tableName, indexName,
                             createDirtyCanonicalLedgerSql(tableName, predicate),
                             busyCancellationObserved);
        }

        bool normalizeExistingSsaNumbersIfNeeded(sqlite3* db, const std::string& tableName,
                                                 const std::vector<domain::ColumnDef>& columns,
                                                 const std::atomic_bool* busyCancellationObserved) {
            const auto numberColumn = std::string{domain::kSsaNumberColumnKey};
            const auto uniqueIndexName = "ux_" + tableName + "_" + numberColumn;
            const auto uniqueIndexSql = createUniqueSsaNumberIndexSql(tableName);
            const bool hasTrustedUniqueIndex = hasExactIndexSql(
                db, tableName, uniqueIndexName, uniqueIndexSql, busyCancellationObserved);
            if (!hasTrustedUniqueIndex &&
                hasNamedIndex(db, uniqueIndexName, busyCancellationObserved)) {
                executeSql(db, "DROP INDEX " + query::quoteTableIdentifier(uniqueIndexName),
                           busyCancellationObserved);
            }
            if (!hasTrustedUniqueIndex) {
                dropLegacySsaNumberIndex(db, tableName, busyCancellationObserved);
                const auto ledgerIndexName = dirtyCanonicalLedgerIndexName(tableName);
                if (hasNamedIndex(db, ledgerIndexName, busyCancellationObserved)) {
                    executeSql(db, "DROP INDEX " + query::quoteTableIdentifier(ledgerIndexName),
                               busyCancellationObserved);
                }
                dropLegacyDirtyCanonicalIndexes(db, tableName, busyCancellationObserved);
                return true;
            }

            const auto predicate = dirtyCanonicalPredicate(columns);
            const auto ledgerIndexName = dirtyCanonicalLedgerIndexName(tableName);
            const auto ledgerSql = createDirtyCanonicalLedgerSql(tableName, predicate);
            if (!hasExactIndexSql(db, tableName, ledgerIndexName, ledgerSql,
                                  busyCancellationObserved)) {
                dropLegacyDirtyCanonicalIndexes(db, tableName, busyCancellationObserved);
                ensureExactIndex(db, tableName, ledgerIndexName, ledgerSql,
                                 busyCancellationObserved);
            }
            return dirtyCanonicalLedgerHasRows(db, tableName, predicate, busyCancellationObserved);
        }

        // Columns used by interactive filters/sorts/distinct lookups. Indexing them
        // turns the common browse/filter/distinct queries from full table scans into
        // index lookups on large datasets. IF NOT EXISTS keeps this idempotent and
        // safe to re-run on existing databases. Only columns actually present in the
        // configured schema are indexed, so custom imports without these columns do
        // not raise "no such column".
        std::vector<std::string>
        createFilterIndexesSql(const std::string& tableName,
                               const std::vector<domain::ColumnDef>& columns) {
            static constexpr std::array filterColumns = {
                std::string_view{"situacao"}, std::string_view{"setor_executor"},
                std::string_view{"derivada_de"}, std::string_view{"semana_programada"},
                std::string_view{"semana_executada"}};
            std::unordered_set<std::string_view> present;
            for (const auto& column : columns) {
                present.insert(column.key);
            }
            std::vector<std::string> statements;
            for (const auto column : filterColumns) {
                if (present.find(column) == present.end()) {
                    continue;
                }
                const auto name = "idx_" + tableName + "_" + std::string{column};
                statements.push_back("CREATE INDEX IF NOT EXISTS " +
                                     query::quoteTableIdentifier(name) + " ON " +
                                     query::quoteTableIdentifier(tableName) + " (" +
                                     query::quoteColumnIdentifier(std::string{column}) + ")");
            }
            return statements;
        }

        std::string insertSql(const std::string& tableName,
                              const std::vector<domain::ColumnDef>& columns) {
            std::ostringstream sql;
            sql << "INSERT INTO " << query::quoteTableIdentifier(tableName) << " (";
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    sql << ", ";
                }
                sql << query::quoteColumnIdentifier(columns[index].key);
            }
            sql << ") VALUES (";
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    sql << ", ";
                }
                sql << "?";
            }
            sql << ")";
            return sql.str();
        }

        std::string selectExistingSql(const std::string& tableName,
                                      const std::vector<domain::ColumnDef>& columns) {
            std::ostringstream sql;
            sql << "SELECT ";
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    sql << ", ";
                }
                sql << query::quoteColumnIdentifier(columns[index].key);
            }
            sql << " FROM " << query::quoteTableIdentifier(tableName) << " WHERE "
                << query::quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey})
                << " = ? LIMIT 1";
            return sql.str();
        }

        std::string updateSql(const std::string& tableName,
                              const std::vector<domain::ColumnDef>& columns) {
            std::ostringstream sql;
            sql << "UPDATE " << query::quoteTableIdentifier(tableName) << " SET ";
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    sql << ", ";
                }
                sql << query::quoteColumnIdentifier(columns[index].key) << " = ?";
            }
            sql << " WHERE "
                << query::quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) << " = ?";
            return sql.str();
        }

        const std::string* rowValuePtr(const importing::SsaImportRow& row, const std::string& key) {
            const auto found = row.find(key);
            return found == row.end() ? nullptr : &found->second;
        }

        bool containsSsaNumberColumn(const std::vector<domain::ColumnDef>& columns) {
            return std::ranges::any_of(columns, [](const domain::ColumnDef& column) {
                return column.key == domain::kSsaNumberColumnKey;
            });
        }

        bool isSsaReferenceColumn(const std::string_view key) {
            return key == "derivada_de" || key.starts_with("numero_ssa_relacionada_");
        }

        void validateCanonicalColumns(const std::vector<domain::ColumnDef>& columns) {
            std::unordered_set<std::string_view> keys;
            keys.reserve(columns.size());
            for (const auto& column : columns) {
                if (!domain::ColumnCatalog::isCanonicalStorageKey(column.key) ||
                    !domain::ColumnCatalog::contains(column.key)) {
                    throw std::invalid_argument(
                        "sqlite import writer received an unknown or non-canonical column");
                }
                if (!keys.insert(column.key).second) {
                    throw std::invalid_argument("sqlite import writer received duplicate columns");
                }
            }
        }

        void validateIdentityColumns(const std::vector<domain::ColumnDef>& columns) {
            int identityColumns = 0;
            for (const auto& column : columns) {
                if (!isIdColumnKey(column.key)) {
                    continue;
                }
                ++identityColumns;
                if (column.type != domain::ColumnType::Integer) {
                    throw std::invalid_argument("sqlite import writer requires integer id column");
                }
            }
            if (identityColumns > 1) {
                throw std::invalid_argument("sqlite import writer received duplicate id columns");
            }
        }

        bool parseInteger(const std::string& value, long long& parsed) {
            const auto* begin = value.data();
            const auto* end = value.data() + value.size();
            const auto [ptr, ec] = std::from_chars(begin, end, parsed);
            return ec == std::errc{} && ptr == end;
        }

        void bindValue(SqliteStatement& statement, const int index, const domain::ColumnDef& column,
                       const std::string* value) {
            if (value == nullptr) {
                statement.bindNullOneBased(index);
                return;
            }
            if (value->empty() && column.type == domain::ColumnType::Integer) {
                statement.bindNullOneBased(index);
                return;
            }
            if (column.type == domain::ColumnType::Integer) {
                long long parsed = 0;
                if (parseInteger(*value, parsed)) {
                    statement.bindInt64OneBased(index, parsed);
                    return;
                }
                throw ports::OperationError("Falha ao validar valor numerico importado",
                                            "invalid integer value for column " + column.key);
            }
            statement.bindTextOneBased(index, *value);
        }

        bool normalizeDateValues(importing::SsaImportRow& row,
                                 const std::vector<domain::ColumnDef>& columns) {
            bool changed = false;
            for (const auto& column : columns) {
                if (column.type != domain::ColumnType::DateText) {
                    continue;
                }
                const auto value = row.find(column.key);
                if (value == row.end() || value->second.empty()) {
                    continue;
                }
                const auto normalized = domain::SsaImportPolicy::normalizeDateText(value->second);
                if (!normalized.empty() && normalized != value->second) {
                    value->second = normalized;
                    changed = true;
                }
            }
            return changed;
        }

        std::size_t normalizeExistingSsaNumbers(sqlite3* db, const std::string& tableName,
                                                const std::vector<domain::ColumnDef>& columns,
                                                const std::stop_token& stopToken,
                                                const std::atomic_bool* busyCancellationObserved) {
            const auto numberColumn =
                query::quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey});
            std::vector<std::string> referenceColumns;
            std::string selectSql = "SELECT rowid, " + numberColumn;
            for (const auto& column : columns) {
                if (!isSsaReferenceColumn(column.key)) {
                    continue;
                }
                referenceColumns.push_back(column.key);
                selectSql += ", " + query::quoteColumnIdentifier(column.key);
            }
            selectSql += " FROM " + query::quoteTableIdentifier(tableName);
            executeSql(db,
                       "CREATE TEMP TABLE ssa_import_normalized_numbers (normalized TEXT "
                       "PRIMARY KEY NOT NULL)",
                       busyCancellationObserved);
            SqliteStatement seen(
                db, "INSERT OR IGNORE INTO ssa_import_normalized_numbers(normalized) VALUES(?)",
                busyCancellationObserved);
            std::string normalizationSql = "UPDATE " + query::quoteTableIdentifier(tableName) +
                                           " SET " + numberColumn + " = ?";
            for (const auto& referenceColumn : referenceColumns) {
                const auto quoted = query::quoteColumnIdentifier(referenceColumn);
                normalizationSql.append(", ")
                    .append(quoted)
                    .append(" = CASE WHEN ")
                    .append(quoted)
                    .append(" IS NOT NULL THEN ? ELSE ")
                    .append(quoted)
                    .append(" END");
            }
            normalizationSql += " WHERE rowid = ?";
            SqliteStatement update(db, normalizationSql, busyCancellationObserved);
            SqliteStatement select(db, selectSql, busyCancellationObserved);
            std::size_t changedRows = 0;
            while (select.step()) {
                throwIfCanceled(stopToken);
                const auto rowId = select.columnInt64(0);
                const int numberStorageType = sqlite3_column_type(select.handle(), 1);
                const auto rawNumber = select.columnText(1);
                const auto normalizedNumber = domain::SsaImportPolicy::normalizeNumber(rawNumber);
                if (normalizedNumber.empty()) {
                    throw ports::OperationError("Falha ao validar identificadores SSA existentes",
                                                "invalid SSA number in existing database");
                }
                seen.bindTextOneBased(1, normalizedNumber);
                seen.executeAndReset();
                if (sqlite3_changes(db) == 0) {
                    throw ports::OperationError("Falha ao validar identificadores SSA existentes",
                                                "semantic SSA collision in existing database");
                }
                std::vector<std::string> rawReferences;
                std::vector<std::string> normalizedReferences;
                rawReferences.reserve(referenceColumns.size());
                normalizedReferences.reserve(referenceColumns.size());
                bool requiresStorageNormalization = numberStorageType != SQLITE_TEXT;
                for (std::size_t index = 0; index < referenceColumns.size(); ++index) {
                    const auto columnIndex = static_cast<int>(index + 2);
                    const int storageType = sqlite3_column_type(select.handle(), columnIndex);
                    auto raw = select.columnText(columnIndex);
                    const auto trimmed = domain::trimWhitespace(raw);
                    auto normalized = trimmed.empty()
                                          ? std::string{}
                                          : domain::SsaImportPolicy::normalizeNumber(raw);
                    if (!trimmed.empty() && normalized.empty()) {
                        throw ports::OperationError("Falha ao validar referencias SSA existentes",
                                                    "invalid SSA reference in existing database");
                    }
                    requiresStorageNormalization =
                        requiresStorageNormalization ||
                        (storageType != SQLITE_NULL && storageType != SQLITE_TEXT);
                    rawReferences.push_back(std::move(raw));
                    normalizedReferences.push_back(std::move(normalized));
                }
                if (!requiresStorageNormalization && rawNumber == normalizedNumber &&
                    rawReferences == normalizedReferences) {
                    continue;
                }
                int bindIndex = 1;
                update.bindTextOneBased(bindIndex++, normalizedNumber);
                for (const auto& normalizedReference : normalizedReferences) {
                    update.bindTextOneBased(bindIndex++, normalizedReference);
                }
                update.bindInt64OneBased(bindIndex, rowId);
                update.executeAndReset();
                changedRows += static_cast<std::size_t>(sqlite3_changes(db));
            }
            return changedRows;
        }

    } // namespace

    SqliteSsaImportWriter::SqliteSsaImportWriter(SqliteSsaImportWriterAccess,
                                                 std::filesystem::path databasePath,
                                                 std::vector<domain::ColumnDef> columns,
                                                 std::string tableName)
        : databasePath_(std::move(databasePath)), columns_(std::move(columns)),
          tableName_(std::move(tableName)) {
        if (!isValidSqlIdentifier(tableName_)) {
            throw std::invalid_argument("invalid sqlite import table name");
        }
        if (columns_.empty()) {
            throw std::invalid_argument("sqlite import writer requires columns");
        }
        validateCanonicalColumns(columns_);
        validateIdentityColumns(columns_);
        if (!containsSsaNumberColumn(columns_)) {
            throw std::invalid_argument("sqlite import writer requires " +
                                        std::string{domain::kSsaNumberColumnKey} + " column");
        }
    }

    struct SqliteSsaImportWriter::WriteSession::Storage final {
        enum class State {
            Active,
            Committed,
            RolledBack,
        };

        Storage(const std::filesystem::path& databasePath,
                const std::vector<domain::ColumnDef>& configuredColumns, std::string tableName,
                const bool replaceAll, std::stop_token stopToken,
                const std::chrono::milliseconds sqliteBusyWait)
            : connection(databasePath, SqliteOpenMode::ReadWriteCreate, sqliteBusyWait),
              busy(connection.handle(), stopToken, sqliteBusyWait),
              progress(connection.handle(), stopToken), stopToken(std::move(stopToken)),
              columns(configuredColumns), tableName(std::move(tableName)) {
            auto* db = connection.handle();
            transaction = std::make_unique<SqliteWriteTransaction>(db, busy.cancellationObserved());
            try {
                const bool stampLegacySchemaVersion =
                    usesLegacySchemaVersion(db, busy.cancellationObserved());
                ensureConsolidationJournalSchema(db, busy.cancellationObserved());
                executeSql(db, createTableSql(this->tableName, columns),
                           busy.cancellationObserved());
                ensureDerivedCountSummary(db, this->tableName, columns,
                                          busy.cancellationObserved());
                bool normalizedExistingRows = false;
                if (replaceAll) {
                    dropLegacySsaNumberIndex(db, this->tableName, busy.cancellationObserved());
                    executeSql(db, "DELETE FROM " + query::quoteTableIdentifier(this->tableName),
                               busy.cancellationObserved());
                } else {
                    if (normalizeExistingSsaNumbersIfNeeded(db, this->tableName, columns,
                                                            busy.cancellationObserved())) {
                        const auto normalizedRows = normalizeExistingSsaNumbers(
                            db, this->tableName, columns, this->stopToken,
                            busy.cancellationObserved());
                        summary.rowsWritten += normalizedRows;
                        summary.rowsUpdated += normalizedRows;
                        normalizedExistingRows = true;
                    }
                }
                const auto uniqueIndexName =
                    "ux_" + this->tableName + "_" + std::string{domain::kSsaNumberColumnKey};
                ensureExactIndex(db, this->tableName, uniqueIndexName,
                                 createUniqueSsaNumberIndexSql(this->tableName),
                                 busy.cancellationObserved());
                if (replaceAll || normalizedExistingRows) {
                    if (replaceAll) {
                        dropLegacyDirtyCanonicalIndexes(db, this->tableName,
                                                        busy.cancellationObserved());
                    }
                    ensureDirtyCanonicalLedger(db, this->tableName, columns,
                                               busy.cancellationObserved());
                }
                const auto dirtyPredicate = dirtyCanonicalPredicate(columns);
                if (dirtyCanonicalLedgerHasRows(db, this->tableName, dirtyPredicate,
                                                busy.cancellationObserved())) {
                    throw ports::OperationError(
                        "Falha ao validar identificadores SSA existentes",
                        "dirty canonical ledger still contains rows after normalization");
                }
                for (const auto& indexSql : createFilterIndexesSql(this->tableName, columns)) {
                    executeSql(db, indexSql, busy.cancellationObserved());
                }
                if (stampLegacySchemaVersion) {
                    executeSql(db,
                               "PRAGMA user_version = " +
                                   std::to_string(domain::ColumnCatalog::schemaVersion()),
                               busy.cancellationObserved());
                }
                insert = std::make_unique<SqliteStatement>(db, insertSql(this->tableName, columns),
                                                           busy.cancellationObserved());
                selectExisting = std::make_unique<SqliteStatement>(
                    db, selectExistingSql(this->tableName, columns), busy.cancellationObserved());
                update = std::make_unique<SqliteStatement>(db, updateSql(this->tableName, columns),
                                                           busy.cancellationObserved());
            } catch (...) {
                rollback();
                throw;
            }
        }

        ~Storage() {
            if (transaction == nullptr || !transaction->active()) {
                return;
            }
            progress.disable();
            busy.disable();
            try {
                transaction->rollback();
            } catch (const ports::OperationError& error) {
                sqlite3_log(SQLITE_ERROR, "sqlite import rollback failed: %s",
                            error.diagnostic().c_str());
            } catch (const std::exception& error) {
                sqlite3_log(SQLITE_ERROR, "sqlite import rollback failed: %s", error.what());
            }
        }

        [[nodiscard]] importing::SsaImportBatchWriteSummary
        write(const importing::ResolvedSsaImportRows& rows,
              const importing::SsaImportWriteSummary& batchSummary) {
            if (state != State::Active) {
                throw std::logic_error("sqlite import session is closed");
            }
            throwIfCanceled(stopToken);
            summary.files += batchSummary.files;
            summary.skippedRows += batchSummary.skippedRows;
            summary.duplicateRows += rows.duplicateRows;
            summary.conflictRows += rows.conflictRows;
            importing::SsaImportBatchWriteSummary result;
            result.duplicateRows = rows.duplicateRows;
            result.conflictRows = rows.conflictRows;
            const auto ssaNumberKey = std::string{domain::kSsaNumberColumnKey};

            for (const auto& row : rows.rows) {
                throwIfCanceled(stopToken);
                auto normalizedRow = row;
                const auto number = domain::SsaImportPolicy::normalizeNumber(
                    importing::rowValue(row, ssaNumberKey));
                if (number.empty()) {
                    throw ports::OperationError("Falha ao validar identificador SSA importado",
                                                "invalid SSA number in import batch");
                }
                if (!seenImportedNumbers.insert(number).second) {
                    ++summary.duplicateRows;
                    ++result.duplicateRows;
                }
                normalizedRow[ssaNumberKey] = number;
                for (const auto& column : columns) {
                    if (!isSsaReferenceColumn(column.key)) {
                        continue;
                    }
                    const auto reference = normalizedRow.find(column.key);
                    if (reference == normalizedRow.end() || reference->second.empty()) {
                        continue;
                    }
                    const auto normalizedReference =
                        domain::SsaImportPolicy::normalizeNumber(reference->second);
                    if (normalizedReference.empty()) {
                        throw ports::OperationError("Falha ao validar referencia SSA importada",
                                                    "invalid SSA reference in import batch");
                    }
                    reference->second = normalizedReference;
                }
                selectExisting->bindTextOneBased(1, number);
                domain::SsaImportPolicy::Values existing;
                if (selectExisting->step()) {
                    for (int index = 0; index < selectExisting->columnCount(); ++index) {
                        if (sqlite3_column_type(selectExisting->handle(), index) != SQLITE_NULL) {
                            existing.emplace(selectExisting->columnName(index),
                                             selectExisting->columnText(index));
                        }
                    }
                }
                selectExisting->resetAndClearBindings();
                const bool normalizedExistingDates = normalizeDateValues(existing, columns);

                const auto merged = domain::SsaImportPolicy::merge(existing, normalizedRow);
                if (merged.conflict) {
                    ++summary.conflictRows;
                    ++result.conflictRows;
                    return result;
                }
                if (!existing.empty() && !merged.changed && !normalizedExistingDates) {
                    ++summary.rowsUnchanged;
                    ++result.rowsUnchanged;
                    continue;
                }
                auto& statement = existing.empty() ? *insert : *update;
                int bindIndex = 1;
                for (const auto& column : columns) {
                    bindValue(statement, bindIndex, column, rowValuePtr(merged.values, column.key));
                    ++bindIndex;
                }
                if (!existing.empty()) {
                    update->bindTextOneBased(bindIndex, number);
                }
                statement.executeAndReset();
                ++summary.rowsWritten;
                ++result.rowsWritten;
                if (existing.empty()) {
                    ++summary.rowsInserted;
                    ++result.rowsInserted;
                } else {
                    ++summary.rowsUpdated;
                    ++result.rowsUpdated;
                }
            }
            return result;
        }

        [[nodiscard]] importing::SsaImportWriteSummary finish() {
            if (state == State::RolledBack) {
                throw std::logic_error("sqlite import session was rolled back");
            }
            throwIfCanceled(stopToken);
            if (state == State::Active) {
                transaction->commit();
                state = State::Committed;
            }
            return summary;
        }

        [[nodiscard]] importing::SsaImportWriteSummary
        finishWithAnalytics(const int observedIsoYearWeek, std::string observedDate) {
            if (state != State::Active) {
                throw std::logic_error("sqlite import session is closed");
            }
            const auto fingerprint = SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
                connection.handle(), tableName, stopToken, busy.cancellationObserved());
            const ActivityAnalyticsCaptureContext context{
                .observedIsoYearWeek = observedIsoYearWeek,
                .observedDate = std::move(observedDate),
                .sourceRevision = fingerprint,
                .sourceFingerprint = fingerprint,
            };
            static_cast<void>(SqliteActivityAnalyticsProjection::capture(
                connection.handle(), tableName, context, stopToken, busy.cancellationObserved()));
            return finish();
        }

        void recordConsolidation(const std::vector<importing::ImportConsolidationMove>& moves) {
            if (state != State::Active) {
                throw std::logic_error("sqlite import session is closed");
            }
            throwIfCanceled(stopToken);
            SqliteStatement insertJournal(
                connection.handle(),
                "INSERT INTO " + std::string{kConsolidationJournalTable} +
                    "(source, destination, has_valid_rows, source_identity, source_size) "
                    "VALUES(?, ?, ?, ?, ?)",
                busy.cancellationObserved());
            for (const auto& move : moves) {
                throwIfCanceled(stopToken);
                const auto source = std::filesystem::absolute(move.source).lexically_normal();
                const auto destination =
                    std::filesystem::absolute(move.destination).lexically_normal();
                if (source.empty() || destination.empty()) {
                    throw std::invalid_argument("consolidation journal paths cannot be empty");
                }
                insertJournal.bindTextOneBased(1, qt::toUtf8(source));
                insertJournal.bindTextOneBased(2, qt::toUtf8(destination));
                insertJournal.bindInt64OneBased(3, move.hasValidRows ? 1 : 0);
                std::error_code identityError;
                const auto observed = importing::inspectFileIdentity(source, identityError);
                if (!observed) {
                    throw std::runtime_error("cannot inspect consolidation source identity: " +
                                             identityError.message());
                }
                if ((!move.sourceIdentity.empty() && move.sourceIdentity != observed->value) ||
                    (move.sourceSize && *move.sourceSize != observed->size)) {
                    throw std::runtime_error("consolidation source changed before journal commit");
                }
                insertJournal.bindTextOneBased(4, observed->value);
                insertJournal.bindInt64OneBased(5, static_cast<std::int64_t>(observed->size));
                insertJournal.executeAndReset();
            }
        }

        void rollback() {
            if (state != State::Active) {
                return;
            }
            state = State::RolledBack;
            if (transaction == nullptr || !transaction->active()) {
                return;
            }
            progress.disable();
            busy.disable();
            transaction->rollback();
        }

        SqliteConnection connection;
        SqliteBusyHandler busy;
        SqliteProgressHandler progress;
        std::stop_token stopToken;
        std::unique_ptr<SqliteWriteTransaction> transaction;
        std::vector<domain::ColumnDef> columns;
        std::string tableName;
        std::unique_ptr<SqliteStatement> insert;
        std::unique_ptr<SqliteStatement> selectExisting;
        std::unique_ptr<SqliteStatement> update;
        std::unordered_set<std::string> seenImportedNumbers;
        importing::SsaImportWriteSummary summary;
        State state = State::Active;
    };

    SqliteSsaImportWriter::WriteSession::WriteSession(std::unique_ptr<Storage> storage)
        : storage_(std::move(storage)) {}

    SqliteSsaImportWriter::WriteSession::~WriteSession() = default;

    SqliteSsaImportWriter::WriteSession::WriteSession(WriteSession&&) noexcept = default;

    SqliteSsaImportWriter::WriteSession&
    SqliteSsaImportWriter::WriteSession::operator=(WriteSession&&) noexcept = default;

    importing::SsaImportBatchWriteSummary
    SqliteSsaImportWriter::WriteSession::write(const importing::ResolvedSsaImportRows& rows,
                                               const std::size_t fileCount,
                                               const std::size_t skippedRows) {
        const importing::SsaImportWriteSummary batchSummary{.files = fileCount,
                                                            .skippedRows = skippedRows};
        return storage_->write(rows, batchSummary);
    }

    void SqliteSsaImportWriter::WriteSession::recordConsolidation(
        const std::vector<importing::ImportConsolidationMove>& moves) {
        storage_->recordConsolidation(moves);
    }

    importing::SsaImportWriteSummary SqliteSsaImportWriter::WriteSession::finish() {
        return storage_->finish();
    }

    importing::SsaImportWriteSummary
    SqliteSsaImportWriter::WriteSession::finishWithAnalytics(const int observedIsoYearWeek,
                                                             std::string observedDate) {
        return storage_->finishWithAnalytics(observedIsoYearWeek, std::move(observedDate));
    }

    void SqliteSsaImportWriter::WriteSession::rollback() {
        storage_->rollback();
    }

    importing::SsaImportWriteSummary
    SqliteSsaImportWriter::write(const importing::ResolvedSsaImportRows& rows,
                                 const std::size_t fileCount, const std::size_t skippedRows,
                                 const bool replaceAll, std::stop_token stopToken) const {
        auto session = startSession(replaceAll, std::move(stopToken));
        if (session.write(rows, fileCount, skippedRows).conflictRows > 0) {
            session.rollback();
            throw ports::OperationError("SSA import was rejected", "duplicate_conflict");
        }
        return session.finish();
    }

    SqliteSsaImportWriter::WriteSession
    SqliteSsaImportWriter::startSession(const bool replaceAll, std::stop_token stopToken,
                                        const std::chrono::milliseconds sqliteBusyWait) const {
        throwIfCanceled(stopToken);
        return WriteSession{std::make_unique<WriteSession::Storage>(
            databasePath_, columns_, tableName_, replaceAll, std::move(stopToken), sqliteBusyWait)};
    }

    std::vector<importing::ImportConsolidationMove> SqliteSsaImportWriter::pendingConsolidation(
        const std::stop_token& stopToken, const std::chrono::milliseconds sqliteBusyWait) const {
        throwIfCanceled(stopToken);
        std::error_code error;
        const bool databaseExists = std::filesystem::exists(databasePath_, error);
        if (error) {
            throw std::runtime_error("cannot inspect sqlite database: " + error.message());
        }
        if (!databaseExists) {
            return {};
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadOnly, sqliteBusyWait);
        SqliteBusyHandler busy(connection.handle(), stopToken, sqliteBusyWait);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteReadTransaction transaction(connection.handle(), stopToken,
                                          busy.cancellationObserved());
        static_cast<void>(
            usesLegacySchemaVersion(connection.handle(), busy.cancellationObserved()));
        SqliteStatement tableExists(
            connection.handle(), "SELECT COUNT(*) FROM sqlite_schema WHERE type='table' AND name=?",
            busy.cancellationObserved());
        tableExists.bindTextOneBased(1, std::string{kConsolidationJournalTable});
        if (!tableExists.step() || tableExists.columnInt64(0) == 0) {
            return {};
        }
        const auto identityExpression =
            consolidationJournalHasColumn(connection.handle(), "source_identity",
                                          busy.cancellationObserved())
                ? "source_identity"
                : "NULL";
        const auto sizeExpression =
            consolidationJournalHasColumn(connection.handle(), "source_size",
                                          busy.cancellationObserved())
                ? "source_size"
                : "NULL";
        SqliteStatement select(connection.handle(),
                               "SELECT source, destination, has_valid_rows, " +
                                   std::string{identityExpression} + ", " +
                                   std::string{sizeExpression} + " FROM " +
                                   std::string{kConsolidationJournalTable} + " ORDER BY source",
                               busy.cancellationObserved());
        std::vector<importing::ImportConsolidationMove> pending;
        while (select.step()) {
            throwIfCanceled(stopToken);
            std::optional<std::uintmax_t> sourceSize;
            if (sqlite3_column_type(select.handle(), 4) != SQLITE_NULL) {
                sourceSize = static_cast<std::uintmax_t>(select.columnInt64(4));
            }
            pending.push_back({journalPath(select.columnText(0)), journalPath(select.columnText(1)),
                               select.columnInt64(2) != 0, select.columnText(3), sourceSize});
        }
        transaction.commit();
        return pending;
    }

    void SqliteSsaImportWriter::completeConsolidation(
        const std::vector<importing::ImportConsolidationMove>& moves,
        const std::chrono::milliseconds sqliteBusyWait) const {
        if (moves.empty()) {
            return;
        }
        SqliteConnection connection(databasePath_, SqliteOpenMode::ReadWrite, sqliteBusyWait);
        SqliteBusyHandler busy(connection.handle(), {}, sqliteBusyWait);
        SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());
        static_cast<void>(
            usesLegacySchemaVersion(connection.handle(), busy.cancellationObserved()));
        ensureConsolidationJournalSchema(connection.handle(), busy.cancellationObserved());
        SqliteStatement erase(connection.handle(),
                              "DELETE FROM " + std::string{kConsolidationJournalTable} +
                                  " WHERE source=? AND destination=? AND source_identity IS ? "
                                  "AND source_size IS ?",
                              busy.cancellationObserved());
        for (const auto& move : moves) {
            erase.bindTextOneBased(
                1, qt::toUtf8(std::filesystem::absolute(move.source).lexically_normal()));
            erase.bindTextOneBased(
                2, qt::toUtf8(std::filesystem::absolute(move.destination).lexically_normal()));
            if (move.sourceIdentity.empty()) {
                erase.bindNullOneBased(3);
            } else {
                erase.bindTextOneBased(3, move.sourceIdentity);
            }
            if (move.sourceSize) {
                erase.bindInt64OneBased(4, static_cast<std::int64_t>(*move.sourceSize));
            } else {
                erase.bindNullOneBased(4);
            }
            erase.executeAndReset();
            if (sqlite3_changes(connection.handle()) != 1) {
                throw ports::OperationError("Falha ao concluir a consolidacao da importacao",
                                            "consolidation journal entry was not removed");
            }
        }
        transaction.commit();
    }

} // namespace ssa::infra::sqlite
