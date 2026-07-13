#include "infra/sqlite/SqliteSsaImportWriter.h"

#include "domain/SsaTypes.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "query/SqlQueryText.h"

#include <sqlite3.h>

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

        std::string ssaNumberIndexName(const std::string& tableName) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "idx_" + tableName + "_" + ssaNumberColumn;
        }

        std::string createSsaNumberIndexSql(const std::string& tableName) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "CREATE INDEX IF NOT EXISTS " +
                   query::quoteTableIdentifier(ssaNumberIndexName(tableName)) + " ON " +
                   query::quoteTableIdentifier(tableName) + " (" +
                   query::quoteColumnIdentifier(ssaNumberColumn) + ")";
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

        const std::string* rowValuePtr(const importing::SsaImportRow& row, const std::string& key) {
            const auto found = row.find(key);
            return found == row.end() ? nullptr : &found->second;
        }

        bool containsSsaNumberColumn(const std::vector<domain::ColumnDef>& columns) {
            return std::ranges::any_of(columns, [](const domain::ColumnDef& column) {
                return column.key == domain::kSsaNumberColumnKey;
            });
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
            }
            statement.bindTextOneBased(index, *value);
        }

        void prepareImportNumberTable(sqlite3* db) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            executeSql(db, "CREATE TEMP TABLE IF NOT EXISTS temp_ssa_import_numbers (" +
                               query::quoteColumnIdentifier(ssaNumberColumn) +
                               " TEXT PRIMARY KEY)");
        }

        std::string insertImportNumberSql() {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "INSERT OR IGNORE INTO temp_ssa_import_numbers(" +
                   query::quoteColumnIdentifier(ssaNumberColumn) + ") VALUES (?)";
        }

        std::string deleteExistingRowsSql(const std::string& tableName) {
            const auto ssaNumberColumn = std::string{domain::kSsaNumberColumnKey};
            return "DELETE FROM " + query::quoteTableIdentifier(tableName) + " WHERE " +
                   query::quoteColumnIdentifier(ssaNumberColumn) + " IN (SELECT " +
                   query::quoteColumnIdentifier(ssaNumberColumn) + " FROM temp_ssa_import_numbers)";
        }

        void deleteExistingRowsByNumber(sqlite3* db, const std::vector<std::string>& numbers,
                                        SqliteStatement& insertNumber,
                                        SqliteStatement& deleteExistingRows) {
            if (numbers.empty()) {
                return;
            }
            executeSql(db, "DELETE FROM temp_ssa_import_numbers");
            for (const auto& value : numbers) {
                if (value.empty()) {
                    continue;
                }
                insertNumber.bindTextOneBased(1, value);
                insertNumber.executeAndReset();
            }
            deleteExistingRows.executeAndReset();
        }

    } // namespace

    SqliteSsaImportWriter::SqliteSsaImportWriter(std::filesystem::path databasePath,
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
                const bool replaceAll, std::stop_token stopToken)
            : connection(databasePath, SqliteOpenMode::ReadWriteCreate),
              busy(connection.handle(), stopToken), progress(connection.handle(), stopToken),
              stopToken(std::move(stopToken)), columns(configuredColumns),
              tableName(std::move(tableName)) {
            auto* db = connection.handle();
            transaction = std::make_unique<SqliteWriteTransaction>(db, busy.cancellationObserved());
            try {
                executeSql(db, createTableSql(this->tableName, columns),
                           busy.cancellationObserved());
                executeSql(db, createSsaNumberIndexSql(this->tableName),
                           busy.cancellationObserved());
                for (const auto& indexSql : createFilterIndexesSql(this->tableName, columns)) {
                    executeSql(db, indexSql, busy.cancellationObserved());
                }
                prepareImportNumberTable(db);
                if (replaceAll) {
                    executeSql(db, "DELETE FROM " + query::quoteTableIdentifier(this->tableName),
                               busy.cancellationObserved());
                }
                insert = std::make_unique<SqliteStatement>(db, insertSql(this->tableName, columns),
                                                           busy.cancellationObserved());
                insertNumber = std::make_unique<SqliteStatement>(db, insertImportNumberSql(),
                                                                 busy.cancellationObserved());
                deleteExisting = std::make_unique<SqliteStatement>(
                    db, deleteExistingRowsSql(this->tableName), busy.cancellationObserved());
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

        void write(const importing::ResolvedSsaImportRows& rows,
                   const importing::SsaImportWriteSummary& batchSummary) {
            if (state != State::Active) {
                throw std::logic_error("sqlite import session is closed");
            }
            throwIfCanceled(stopToken);
            summary.files += batchSummary.files;
            summary.skippedRows += batchSummary.skippedRows;
            summary.duplicateRows += rows.duplicateRows;

            auto* db = connection.handle();
            deleteExistingRowsByNumber(db, rows.ssaNumbersForUpsertDelete, *insertNumber,
                                       *deleteExisting);

            for (const auto& row : rows.rows) {
                throwIfCanceled(stopToken);
                int bindIndex = 1;
                for (const auto& column : columns) {
                    bindValue(*insert, bindIndex, column, rowValuePtr(row, column.key));
                    ++bindIndex;
                }
                insert->executeAndReset();
                ++summary.rowsWritten;
            }
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
        std::unique_ptr<SqliteStatement> insertNumber;
        std::unique_ptr<SqliteStatement> deleteExisting;
        importing::SsaImportWriteSummary summary;
        State state = State::Active;
    };

    SqliteSsaImportWriter::WriteSession::WriteSession(std::unique_ptr<Storage> storage)
        : storage_(std::move(storage)) {}

    SqliteSsaImportWriter::WriteSession::~WriteSession() = default;

    SqliteSsaImportWriter::WriteSession::WriteSession(WriteSession&&) noexcept = default;

    SqliteSsaImportWriter::WriteSession&
    SqliteSsaImportWriter::WriteSession::operator=(WriteSession&&) noexcept = default;

    void SqliteSsaImportWriter::WriteSession::write(const importing::ResolvedSsaImportRows& rows,
                                                    const std::size_t fileCount,
                                                    const std::size_t skippedRows) {
        const importing::SsaImportWriteSummary batchSummary{.files = fileCount,
                                                            .skippedRows = skippedRows};
        storage_->write(rows, batchSummary);
    }

    importing::SsaImportWriteSummary SqliteSsaImportWriter::WriteSession::finish() {
        return storage_->finish();
    }

    void SqliteSsaImportWriter::WriteSession::rollback() {
        storage_->rollback();
    }

    importing::SsaImportWriteSummary
    SqliteSsaImportWriter::write(const importing::ResolvedSsaImportRows& rows,
                                 const std::size_t fileCount, const std::size_t skippedRows,
                                 const bool replaceAll, std::stop_token stopToken) const {
        auto session = startSession(replaceAll, std::move(stopToken));
        session.write(rows, fileCount, skippedRows);
        return session.finish();
    }

    SqliteSsaImportWriter::WriteSession
    SqliteSsaImportWriter::startSession(const bool replaceAll, std::stop_token stopToken) const {
        throwIfCanceled(stopToken);
        return WriteSession{std::make_unique<WriteSession::Storage>(
            databasePath_, columns_, tableName_, replaceAll, std::move(stopToken))};
    }

} // namespace ssa::infra::sqlite
