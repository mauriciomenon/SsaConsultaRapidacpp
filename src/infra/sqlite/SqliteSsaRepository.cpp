#include "infra/sqlite/SqliteSsaRepository.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        void bindAll(SqliteStatement& statement, const std::vector<std::string>& bindings) {
            int index = 1;
            for (const auto& binding : bindings) {
                statement.bindTextOneBased(index, binding);
                ++index;
            }
        }

        std::shared_ptr<const domain::SsaRecord::SchemaIndex>
        readSchema(SqliteStatement& statement) {
            const int count = statement.columnCount();
            auto schema = std::make_shared<domain::SsaRecord::SchemaIndex>();
            schema->keys.reserve(static_cast<std::size_t>(count));
            schema->indexByKey.reserve(static_cast<std::size_t>(count));
            for (int column = 0; column < count; ++column) {
                const auto key = statement.columnName(column);
                schema->indexByKey.emplace(key, static_cast<std::size_t>(column));
                schema->keys.push_back(key);
            }
            return schema;
        }

        domain::SsaRecord readRecord(SqliteStatement& statement,
                                     std::shared_ptr<const domain::SsaRecord::SchemaIndex> schema) {
            std::vector<std::string> values(schema->keys.size());
            for (int column = 0; column < statement.columnCount(); ++column) {
                values[static_cast<std::size_t>(column)] = statement.columnText(column);
            }
            return domain::SsaRecord{std::move(schema), std::move(values)};
        }

        void executeSql(sqlite3* db, const std::string_view sql) {
            char* error = nullptr;
            const int rc = sqlite3_exec(db, std::string{sql}.c_str(), nullptr, nullptr, &error);
            if (rc != SQLITE_OK) {
                const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
                sqlite3_free(error);
                throw std::runtime_error("sqlite command failed: " + message);
            }
            sqlite3_free(error);
        }

        class ReadTransaction final {
          public:
            explicit ReadTransaction(sqlite3* db) : db_(db) {
                executeSql(db_, "BEGIN");
            }

            ~ReadTransaction() {
                if (active_) {
                    char* error = nullptr;
                    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &error);
                    sqlite3_free(error);
                }
            }

            ReadTransaction(const ReadTransaction&) = delete;
            ReadTransaction& operator=(const ReadTransaction&) = delete;

            void commit() {
                executeSql(db_, "COMMIT");
                active_ = false;
            }

          private:
            sqlite3* db_;
            bool active_{true};
        };

    } // namespace

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath)
        : SqliteSsaRepository(std::move(dbPath), query::SqlQueryBuilder{}) {}

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath,
                                             query::SqlQueryBuilder queryBuilder)
        : dbPath_(std::move(dbPath)), queryBuilder_(std::move(queryBuilder)) {}

    domain::SsaPageResult SqliteSsaRepository::page(const domain::SsaPageRequest& request) const {
        const auto queries = queryBuilder_.build(request);
        const std::scoped_lock lock(connectionMutex_);
        auto& sqlite = connectionLocked(lock);
        ReadTransaction transaction(sqlite.handle());

        domain::SsaPageResult result;
        result.totalRows = executeCount(sqlite.handle(), queries.count);
        result.pageIndex = request.pageIndex;
        result.pageSize = request.pageSize;
        result.rows = executeRows(sqlite.handle(), queries.page);
        transaction.commit();

        return result;
    }

    std::size_t SqliteSsaRepository::count(const domain::SsaPageRequest& request) const {
        const auto queries = queryBuilder_.build(request);
        const std::scoped_lock lock(connectionMutex_);
        auto& sqlite = connectionLocked(lock);
        return executeCount(sqlite.handle(), queries.count);
    }

    std::optional<domain::SsaRecord>
    SqliteSsaRepository::recordBySsaNumber(const domain::SsaNumber& number) const {
        const auto query = queryBuilder_.buildRecordBySsaNumber(number);
        const std::scoped_lock lock(connectionMutex_);
        auto& sqlite = connectionLocked(lock);
        SqliteStatement statement(sqlite.handle(), query.record.sql);
        bindAll(statement, query.record.bindings);
        if (!statement.step()) {
            return std::nullopt;
        }
        return readRecord(statement, readSchema(statement));
    }

    std::vector<std::string>
    SqliteSsaRepository::distinctValues(const domain::DistinctValuesRequest& request) const {
        const auto query = queryBuilder_.buildDistinctValues(request);
        const std::scoped_lock lock(connectionMutex_);
        auto& sqlite = connectionLocked(lock);
        SqliteStatement statement(sqlite.handle(), query.sql);
        bindAll(statement, query.bindings);

        std::vector<std::string> values;
        while (statement.step()) {
            const auto value = statement.columnText(0);
            if (!value.empty()) {
                values.push_back(value);
            }
        }
        return values;
    }

    ports::SsaReadResult SqliteSsaRepository::readAll(const domain::SsaPageRequest& request,
                                                      ports::SsaRecordConsumer consume) const {
        const auto query = queryBuilder_.buildRows(request);
        const std::scoped_lock lock(connectionMutex_);
        auto& sqlite = connectionLocked(lock);
        return consumeRows(sqlite.handle(), query, consume);
    }

    SqliteConnection&
    SqliteSsaRepository::connectionLocked(const std::scoped_lock<std::mutex>&) const {
        if (!connection_) {
            connection_ = std::make_unique<SqliteConnection>(dbPath_);
        }
        return *connection_;
    }

    std::size_t SqliteSsaRepository::executeCount(sqlite3* db, const query::SqlQuery& query) const {
        SqliteStatement statement(db, query.sql);
        bindAll(statement, query.bindings);
        if (!statement.step()) {
            throw std::runtime_error("sqlite count query did not return a result");
        }
        return static_cast<std::size_t>(statement.columnInt64(0));
    }

    std::vector<domain::SsaRecord>
    SqliteSsaRepository::executeRows(sqlite3* db, const query::SqlQuery& query) const {
        std::vector<domain::SsaRecord> rows;
        const auto result = consumeRows(db, query, [&rows](domain::SsaRecord row) {
            rows.push_back(std::move(row));
            return std::nullopt;
        });
        if (!result.ok()) {
            throw std::runtime_error(result.error);
        }
        return rows;
    }

    ports::SsaReadResult
    SqliteSsaRepository::consumeRows(sqlite3* db, const query::SqlQuery& query,
                                     const ports::SsaRecordConsumer& consume) const {
        SqliteStatement statement(db, query.sql);
        bindAll(statement, query.bindings);

        std::size_t rowCount = 0;
        std::shared_ptr<const domain::SsaRecord::SchemaIndex> schema;
        while (statement.step()) {
            if (!schema) {
                schema = readSchema(statement);
            }
            auto record = readRecord(statement, schema);
            ++rowCount;
            if (auto error = consume(std::move(record)); error.has_value()) {
                return {rowCount, *error};
            }
        }
        return {rowCount, {}};
    }

} // namespace ssa::infra::sqlite
