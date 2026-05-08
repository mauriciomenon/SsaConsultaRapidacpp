#include "infra/sqlite/SqliteSsaRepository.h"

#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"

#include <stdexcept>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        void bindAll(SqliteStatement& statement, const std::vector<std::string>& bindings) {
            int index = 1;
            for (const auto& binding : bindings) {
                statement.bindText(index, binding);
                ++index;
            }
        }

        domain::SsaRecord readRecord(SqliteStatement& statement) {
            domain::SsaRecord record;
            const int count = statement.columnCount();
            for (int column = 0; column < count; ++column) {
                record.values.emplace(statement.columnName(column), statement.columnText(column));
            }
            return record;
        }

        std::string quoteKnownColumn(const std::string& columnKey) {
            if (!domain::ColumnCatalog::contains(columnKey)) {
                throw std::invalid_argument("unknown column: " + columnKey);
            }
            return "\"" + columnKey + "\"";
        }

    } // namespace

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath)
        : dbPath_(std::move(dbPath)) {}

    domain::SsaPageResult SqliteSsaRepository::page(const domain::SsaPageRequest& request) const {
        const auto queries = queryBuilder_.build(request);
        SqliteConnection connection(dbPath_);

        SqliteStatement countStatement(connection.handle(), queries.count.sql);
        bindAll(countStatement, queries.count.bindings);
        const bool hasCount = countStatement.step();
        if (!hasCount) {
            throw std::runtime_error("sqlite count query returned no rows");
        }

        SqliteStatement pageStatement(connection.handle(), queries.page.sql);
        bindAll(pageStatement, queries.page.bindings);

        domain::SsaPageResult result;
        result.totalRows = static_cast<std::size_t>(countStatement.columnInt64(0));
        result.pageIndex = request.pageIndex;
        result.pageSize = request.pageSize;

        while (pageStatement.step()) {
            result.rows.push_back(readRecord(pageStatement));
        }

        return result;
    }

    std::size_t SqliteSsaRepository::count(const domain::SsaPageRequest& request) const {
        const auto queries = queryBuilder_.build(request);
        SqliteConnection connection(dbPath_);
        SqliteStatement statement(connection.handle(), queries.count.sql);
        bindAll(statement, queries.count.bindings);
        if (!statement.step()) {
            throw std::runtime_error("sqlite count query returned no rows");
        }
        return static_cast<std::size_t>(statement.columnInt64(0));
    }

    std::optional<domain::SsaRecord>
    SqliteSsaRepository::recordById(const domain::SsaId& id) const {
        const auto query = queryBuilder_.buildRecordById(id);
        SqliteConnection connection(dbPath_);
        SqliteStatement statement(connection.handle(), query.record.sql);
        bindAll(statement, query.record.bindings);
        if (!statement.step()) {
            return std::nullopt;
        }
        return readRecord(statement);
    }

    std::vector<std::string>
    SqliteSsaRepository::distinctValues(const domain::DistinctValuesRequest& request) const {
        const auto query = queryBuilder_.buildDistinctValues(request);
        SqliteConnection connection(dbPath_);
        SqliteStatement statement(connection.handle(), query.sql);
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

} // namespace ssa::infra::sqlite
