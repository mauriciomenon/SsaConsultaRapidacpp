#include "infra/sqlite/SqliteSchemaInspector.h"

#include "infra/sqlite/SqliteConnection.h"

#include <utility>

namespace ssa::infra::sqlite {

    SqliteSchemaInspector::SqliteSchemaInspector(std::filesystem::path dbPath)
        : dbPath_(std::move(dbPath)) {}

    bool SqliteSchemaInspector::hasSsaTable() const {
        SqliteConnection connection(dbPath_);
        SqliteStatement statement(
            connection.handle(),
            "SELECT 1 FROM sqlite_master WHERE type IN ('table', 'view') AND name = 'ssa_table'");
        return statement.step();
    }

    std::vector<std::string> SqliteSchemaInspector::ssaColumns() const {
        SqliteConnection connection(dbPath_);
        SqliteStatement statement(connection.handle(), "PRAGMA table_info('ssa_table')");
        std::vector<std::string> columns;
        while (statement.step()) {
            columns.push_back(statement.columnText(1));
        }
        return columns;
    }

} // namespace ssa::infra::sqlite
