#include "infra/sqlite/SqliteConnection.h"

#include <exception>
#include <stdexcept>
#include <system_error>

namespace ssa::infra::sqlite {

    namespace {

        int openFlags(const SqliteOpenMode mode) {
            switch (mode) {
            case SqliteOpenMode::ReadOnly:
                return SQLITE_OPEN_READONLY;
            case SqliteOpenMode::ReadWrite:
                return SQLITE_OPEN_READWRITE;
            case SqliteOpenMode::ReadWriteCreate:
                return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            }
            throw std::invalid_argument("unknown sqlite open mode");
        }

        void closeHandle(sqlite3*& db) {
            if (db != nullptr) {
                sqlite3_close_v2(db);
                db = nullptr;
            }
        }

    } // namespace

    SqliteConnection::SqliteConnection(const std::filesystem::path& dbPath,
                                       const SqliteOpenMode mode, const int busyTimeoutMs) {
        const auto path = dbPath.string();
        const int rc = sqlite3_open_v2(path.c_str(), &db_, openFlags(mode), nullptr);
        if (rc != SQLITE_OK) {
            std::string message =
                db_ == nullptr ? "unknown sqlite open error" : sqlite3_errmsg(db_);
            closeHandle(db_);
            throw std::runtime_error("cannot open sqlite database: " + message);
        }
        if (db_ == nullptr) {
            throw std::runtime_error("cannot open sqlite database: null handle");
        }
        sqlite3_busy_timeout(db_, busyTimeoutMs);
    }

    SqliteConnection::~SqliteConnection() {
        closeHandle(db_);
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

    void SqliteStatement::bindTextOneBased(const int index, const std::string& value) {
        if (index < 1) {
            throw std::invalid_argument("sqlite bind index must be one-based");
        }
        const int rc = sqlite3_bind_text(statement_, index, value.c_str(),
                                         static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("cannot bind sqlite parameter");
        }
    }

    void SqliteStatement::bindNullOneBased(const int index) {
        if (index < 1) {
            throw std::invalid_argument("sqlite bind index must be one-based");
        }
        const int rc = sqlite3_bind_null(statement_, index);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("cannot bind sqlite null parameter");
        }
    }

    void SqliteStatement::bindInt64OneBased(const int index, const long long value) {
        if (index < 1) {
            throw std::invalid_argument("sqlite bind index must be one-based");
        }
        const int rc = sqlite3_bind_int64(statement_, index, value);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("cannot bind sqlite int64 parameter");
        }
    }

    bool SqliteStatement::step() {
        const int rc = sqlite3_step(statement_);
        if (rc == SQLITE_ROW) {
            hasCurrentRow_ = true;
            return true;
        }
        hasCurrentRow_ = false;
        if (rc == SQLITE_DONE) {
            return false;
        }
        if (rc == SQLITE_INTERRUPT) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite query canceled");
        }
        throw std::runtime_error("sqlite statement execution failed");
    }

    void SqliteStatement::executeAndReset() {
        const int rc = sqlite3_step(statement_);
        hasCurrentRow_ = false;
        std::exception_ptr resetError;
        try {
            resetAndClearBindings();
        } catch (...) {
            resetError = std::current_exception();
        }
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("sqlite statement execution failed");
        }
        if (resetError) {
            std::rethrow_exception(resetError);
        }
    }

    void SqliteStatement::resetAndClearBindings() {
        hasCurrentRow_ = false;
        const int resetRc = sqlite3_reset(statement_);
        const int clearRc = sqlite3_clear_bindings(statement_);
        if (resetRc != SQLITE_OK) {
            throw std::runtime_error("sqlite statement reset failed");
        }
        if (clearRc != SQLITE_OK) {
            throw std::runtime_error("sqlite statement binding clear failed");
        }
    }

    sqlite3_stmt* SqliteStatement::handle() const noexcept {
        return statement_;
    }

    int SqliteStatement::columnCount() const {
        return sqlite3_column_count(statement_);
    }

    std::string SqliteStatement::columnName(const int column) const {
        if (column < 0 || column >= sqlite3_column_count(statement_)) {
            throw std::out_of_range("sqlite column index is out of range");
        }
        const char* name = sqlite3_column_name(statement_, column);
        if (name == nullptr) {
            throw std::runtime_error("cannot read sqlite column name");
        }
        return std::string{name};
    }

    std::string SqliteStatement::columnText(const int column) const {
        requireCurrentRow();
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement_, column));
        return text == nullptr ? std::string{} : std::string{text};
    }

    long long SqliteStatement::columnInt64(const int column) const {
        requireCurrentRow();
        return sqlite3_column_int64(statement_, column);
    }

    void SqliteStatement::requireCurrentRow() const {
        if (!hasCurrentRow_) {
            throw std::logic_error("sqlite column access requires current row");
        }
    }

} // namespace ssa::infra::sqlite
