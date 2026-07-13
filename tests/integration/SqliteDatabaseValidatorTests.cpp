#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteDatabaseValidator.h"

#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stop_token>
#include <string>

namespace {

    class TemporaryDirectory final {
      public:
        TemporaryDirectory() {
            path_ = std::filesystem::temp_directory_path() /
                    ("ssa_database_validator_" + std::to_string(++sequence_));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        [[nodiscard]] const std::filesystem::path& path() const {
            return path_;
        }

      private:
        inline static int sequence_ = 0;
        std::filesystem::path path_;
    };

    void executeSql(const std::filesystem::path& path, const std::string& sql) {
        sqlite3* database = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &database) == SQLITE_OK);
        char* error = nullptr;
        const int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &error);
        if (error != nullptr) {
            sqlite3_free(error);
        }
        REQUIRE(result == SQLITE_OK);
        REQUIRE(sqlite3_close(database) == SQLITE_OK);
    }

    std::string compatibleSchema(const bool useAffinityVariants = false,
                                 const std::string& wrongAffinityColumn = {}) {
        std::ostringstream sql;
        sql << "CREATE TABLE ssa_table (";
        auto columns = ssa::domain::ColumnCatalog::storageColumns();
        if (useAffinityVariants) {
            std::ranges::reverse(columns);
        }
        for (std::size_t index = 0; index < columns.size(); ++index) {
            if (index > 0) {
                sql << ", ";
            }
            const auto& column = columns[index];
            sql << '"' << column.key << "\" ";
            if (column.key == wrongAffinityColumn) {
                sql << (column.type == ssa::domain::ColumnType::Integer ? "TEXT" : "INTEGER");
            } else {
                sql << (column.type == ssa::domain::ColumnType::Integer
                            ? (useAffinityVariants ? "BIGINT" : "INTEGER")
                            : (useAffinityVariants ? "VARCHAR(255)" : "TEXT"));
            }
            if (column.key == "id") {
                sql << " PRIMARY KEY";
            }
        }
        if (useAffinityVariants) {
            sql << ", extra_metadata BLOB";
        }
        sql << ')';
        return sql.str();
    }

} // namespace

TEST_CASE("database validator rejects missing and non-regular paths") {
    const TemporaryDirectory temporary;
    const ssa::infra::sqlite::SqliteDatabaseValidator validator;

    const auto missing = validator.validate(temporary.path() / "missing.db");
    REQUIRE(missing.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(missing.message == "O arquivo de banco nao existe");

    const auto directory = validator.validate(temporary.path());
    REQUIRE(directory.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(directory.message == "O caminho selecionado nao e um arquivo regular");
}

TEST_CASE("database validator rejects invalid SQLite and missing table") {
    const TemporaryDirectory temporary;
    const ssa::infra::sqlite::SqliteDatabaseValidator validator;
    const auto invalidPath = temporary.path() / "invalid.db";
    {
        std::ofstream invalid{invalidPath};
        invalid << "not sqlite";
    }

    const auto invalid = validator.validate(invalidPath);
    REQUIRE(invalid.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(invalid.message == "O arquivo nao e um banco SQLite valido");

    const auto missingTablePath = temporary.path() / "missing_table.db";
    executeSql(missingTablePath, "CREATE TABLE another_table (id INTEGER)");
    const auto missingTable = validator.validate(missingTablePath);
    REQUIRE(missingTable.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(missingTable.message == "O banco nao contem a tabela ssa_table");
}

TEST_CASE("database validator rejects incompatible and empty ssa_table") {
    const TemporaryDirectory temporary;
    const ssa::infra::sqlite::SqliteDatabaseValidator validator;

    const auto incompatiblePath = temporary.path() / "incompatible.db";
    executeSql(incompatiblePath, "CREATE TABLE ssa_table (id INTEGER PRIMARY KEY)");
    const auto incompatible = validator.validate(incompatiblePath);
    REQUIRE(incompatible.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(incompatible.message == "Schema incompativel: coluna ausente numero_ssa");

    const auto emptyPath = temporary.path() / "empty.db";
    executeSql(emptyPath, compatibleSchema());
    const auto empty = validator.validate(emptyPath);
    REQUIRE(empty.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(empty.message == "A tabela ssa_table nao contem registros");
}

TEST_CASE("database validator rejects an incompatible column affinity") {
    const TemporaryDirectory temporary;
    const auto databasePath = temporary.path() / "wrong_affinity.db";
    executeSql(databasePath, compatibleSchema(false, "numero_ssa"));

    const ssa::infra::sqlite::SqliteDatabaseValidator validator;
    const auto result = validator.validate(databasePath);

    REQUIRE(result.status == ssa::ports::DatabaseValidationStatus::Invalid);
    REQUIRE(result.message == "Schema incompativel: numero_ssa deve usar afinidade TEXT");
}

TEST_CASE("database validator reports a request canceled before SQLite opens") {
    const TemporaryDirectory temporary;
    std::stop_source stopSource;
    stopSource.request_stop();

    const ssa::infra::sqlite::SqliteDatabaseValidator validator;
    const auto result = validator.validate(temporary.path() / "valid.db", stopSource.get_token());

    REQUIRE(result.status == ssa::ports::DatabaseValidationStatus::Canceled);
    REQUIRE(result.message == "Validacao do banco cancelada");
}

TEST_CASE("database validator accepts a populated compatible database read-only") {
    const TemporaryDirectory temporary;
    const auto databasePath = temporary.path() / "valid.db";
    executeSql(databasePath, compatibleSchema(true));
    executeSql(databasePath, "INSERT INTO ssa_table (id, numero_ssa) VALUES (1, 'SSA-1')");

    std::filesystem::permissions(databasePath, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);
    const ssa::infra::sqlite::SqliteDatabaseValidator validator;
    const auto result = validator.validate(databasePath);

    REQUIRE(result.status == ssa::ports::DatabaseValidationStatus::Valid);
    REQUIRE(result.message.empty());
}
