#include "infra/sqlite/SqliteDatabaseValidator.h"

#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

namespace ssa::infra::sqlite {

    namespace {

        enum class SqliteAffinity {
            Integer,
            Text,
            Other,
        };

        std::string uppercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char character) {
                return static_cast<char>(std::toupper(character));
            });
            return value;
        }

        SqliteAffinity affinity(const std::string& declaredType) {
            const auto type = uppercase(declaredType);
            if (type.find("INT") != std::string::npos) {
                return SqliteAffinity::Integer;
            }
            if (type.find("CHAR") != std::string::npos || type.find("CLOB") != std::string::npos ||
                type.find("TEXT") != std::string::npos) {
                return SqliteAffinity::Text;
            }
            return SqliteAffinity::Other;
        }

        bool hasValidHeader(sqlite3* database) {
            SqliteStatement check(database, "PRAGMA quick_check(1)");
            return check.step() && check.columnText(0) == "ok";
        }

        bool hasSsaTable(sqlite3* database) {
            SqliteStatement table(database,
                                  "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?");
            table.bindTextOneBased(1, std::string{domain::ColumnCatalog::schemaTableName()});
            return table.step();
        }

        std::map<std::string, SqliteAffinity> schema(sqlite3* database) {
            std::map<std::string, SqliteAffinity> columns;
            SqliteStatement tableInfo(
                database,
                "PRAGMA table_info(" + std::string{domain::ColumnCatalog::schemaTableName()} + ")");
            while (tableInfo.step()) {
                columns.emplace(tableInfo.columnText(1), affinity(tableInfo.columnText(2)));
            }
            return columns;
        }

        ports::DatabaseValidationResult validateSchema(sqlite3* database) {
            SqliteStatement version(database, "PRAGMA user_version");
            if (!version.step()) {
                return {ports::DatabaseValidationStatus::Invalid,
                        "Schema incompativel: PRAGMA user_version indisponivel",
                        {}};
            }
            const auto actualVersion = version.columnInt64(0);
            const auto currentVersion = domain::ColumnCatalog::schemaVersion();
            if (actualVersion != 0 && actualVersion != currentVersion) {
                return {ports::DatabaseValidationStatus::Invalid,
                        "Schema incompativel: versao " + std::to_string(actualVersion) +
                            " nao suportada (atual " + std::to_string(currentVersion) + ")",
                        {}};
            }
            const auto actual = schema(database);
            for (const auto& expected : domain::ColumnCatalog::schemaColumns()) {
                const auto found = actual.find(expected.key);
                if (found == actual.end()) {
                    return {ports::DatabaseValidationStatus::Invalid,
                            "Schema incompativel: coluna ausente " + expected.key,
                            {}};
                }
                const auto expectedAffinity = expected.type == domain::ColumnType::Integer
                                                  ? SqliteAffinity::Integer
                                                  : SqliteAffinity::Text;
                if (found->second != expectedAffinity) {
                    const auto name =
                        expectedAffinity == SqliteAffinity::Integer ? "INTEGER" : "TEXT";
                    return {ports::DatabaseValidationStatus::Invalid,
                            "Schema incompativel: " + expected.key + " deve usar afinidade " + name,
                            {}};
                }
            }
            return {ports::DatabaseValidationStatus::Valid, {}, {}};
        }

        bool hasRecords(sqlite3* database) {
            SqliteStatement records(
                database, "SELECT 1 FROM " + std::string{domain::ColumnCatalog::schemaTableName()} +
                              " LIMIT 1");
            return records.step();
        }

    } // namespace

    ports::DatabaseValidationResult
    SqliteDatabaseValidator::validate(const std::filesystem::path& path,
                                      const std::stop_token stopToken) const {
        if (stopToken.stop_requested()) {
            return {ports::DatabaseValidationStatus::Canceled, "Validacao do banco cancelada", {}};
        }
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error || !exists) {
            return {ports::DatabaseValidationStatus::Invalid, "O arquivo de banco nao existe", {}};
        }
        if (!std::filesystem::is_regular_file(path, error) || error) {
            return {ports::DatabaseValidationStatus::Invalid,
                    "O caminho selecionado nao e um arquivo regular",
                    {}};
        }

        try {
            SqliteConnection connection(path, SqliteOpenMode::ReadOnly);
            SqliteProgressHandler progress(connection.handle(), stopToken);
            SqliteReadTransaction transaction(connection.handle(), stopToken);
            if (!hasValidHeader(connection.handle())) {
                return {ports::DatabaseValidationStatus::Invalid,
                        "O arquivo nao e um banco SQLite valido",
                        {}};
            }
            if (!hasSsaTable(connection.handle())) {
                return {ports::DatabaseValidationStatus::Invalid,
                        "O banco nao contem a tabela ssa_table",
                        {}};
            }
            if (auto result = validateSchema(connection.handle()); !result.valid()) {
                return result;
            }
            if (!hasRecords(connection.handle())) {
                return {ports::DatabaseValidationStatus::Invalid,
                        "A tabela ssa_table nao contem registros",
                        {}};
            }
            transaction.commit();
            return {ports::DatabaseValidationStatus::Valid, {}, {}};
        } catch (const std::system_error& exception) {
            if (exception.code() == std::errc::operation_canceled) {
                return {
                    ports::DatabaseValidationStatus::Canceled, "Validacao do banco cancelada", {}};
            }
            return {ports::DatabaseValidationStatus::Invalid,
                    "O arquivo nao e um banco SQLite valido", exception.what()};
        } catch (const ports::OperationError& exception) {
            return {ports::DatabaseValidationStatus::Invalid,
                    "O arquivo nao e um banco SQLite valido", exception.diagnostic()};
        } catch (const std::exception& exception) {
            return {ports::DatabaseValidationStatus::Failed, "Falha ao validar o banco selecionado",
                    exception.what()};
        }
    }

} // namespace ssa::infra::sqlite
