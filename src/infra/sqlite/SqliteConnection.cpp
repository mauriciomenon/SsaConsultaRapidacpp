#include "infra/sqlite/SqliteConnection.h"

#include <regex>
#include <stdexcept>

namespace ssa::infra::sqlite {

    namespace {

        void regexp(sqlite3_context* context, int argc, sqlite3_value** argv) {
            if (argc != 2 || sqlite3_value_type(argv[0]) == SQLITE_NULL ||
                sqlite3_value_type(argv[1]) == SQLITE_NULL) {
                sqlite3_result_int(context, 0);
                return;
            }

            const auto* patternText = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
            const auto* valueText = reinterpret_cast<const char*>(sqlite3_value_text(argv[1]));
            try {
                const std::regex pattern(patternText == nullptr ? "" : patternText,
                                         std::regex_constants::icase);
                const bool matched =
                    std::regex_search(valueText == nullptr ? "" : valueText, pattern);
                sqlite3_result_int(context, matched ? 1 : 0);
            } catch (const std::regex_error&) {
                sqlite3_result_int(context, 0);
            }
        }

    } // namespace

    SqliteConnection::SqliteConnection(const std::filesystem::path& dbPath) {
        const auto path = dbPath.string();
        const int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK) {
            std::string message =
                db_ == nullptr ? "unknown sqlite open error" : sqlite3_errmsg(db_);
            if (db_ != nullptr) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            throw std::runtime_error("cannot open sqlite database: " + message);
        }
        sqlite3_create_function(db_, "REGEXP", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                                &regexp, nullptr, nullptr);
    }

    SqliteConnection::~SqliteConnection() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    sqlite3* SqliteConnection::handle() const noexcept {
        return db_;
    }

    SqliteStatement::SqliteStatement(sqlite3* db, const std::string& sql) {
        const int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &statement_, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("cannot prepare sqlite statement: " +
                                     std::string(sqlite3_errmsg(db)));
        }
    }

    SqliteStatement::~SqliteStatement() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    void SqliteStatement::bindText(const int index, const std::string& value) {
        const int rc = sqlite3_bind_text(statement_, index, value.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("cannot bind sqlite parameter");
        }
    }

    bool SqliteStatement::step() {
        const int rc = sqlite3_step(statement_);
        if (rc == SQLITE_ROW) {
            return true;
        }
        if (rc == SQLITE_DONE) {
            return false;
        }
        throw std::runtime_error("sqlite statement execution failed");
    }

    int SqliteStatement::columnCount() const {
        return sqlite3_column_count(statement_);
    }

    std::string SqliteStatement::columnName(const int column) const {
        const char* name = sqlite3_column_name(statement_, column);
        return name == nullptr ? std::string{} : std::string{name};
    }

    std::string SqliteStatement::columnText(const int column) const {
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement_, column));
        return text == nullptr ? std::string{} : std::string{text};
    }

    long long SqliteStatement::columnInt64(const int column) const {
        return sqlite3_column_int64(statement_, column);
    }

} // namespace ssa::infra::sqlite
