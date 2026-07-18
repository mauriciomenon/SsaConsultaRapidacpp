#include "infra/sqlite/SqliteConnection.h"

#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <exception>
#include <iostream>
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
            if (db == nullptr) {
                return;
            }
            const bool hasOutstandingStatements = sqlite3_next_stmt(db, nullptr) != nullptr;
            const char* errorMessage = sqlite3_errmsg(db);
            const int rc = sqlite3_close_v2(db);
            if (rc != SQLITE_OK) {
                std::clog << "sqlite close failed: rc=" << rc
                          << " message=" << (errorMessage == nullptr ? "unknown" : errorMessage)
                          << '\n';
            } else if (hasOutstandingStatements) {
                std::clog << "sqlite close deferred: statements remain open\n";
            }
            db = nullptr;
        }

        bool isCanceledResult(const int rc, const std::atomic_bool* busyCancellationObserved) {
            if (rc == SQLITE_INTERRUPT) {
                return true;
            }
            const bool isBusy = rc == SQLITE_BUSY || rc == SQLITE_LOCKED;
            return isBusy && busyCancellationObserved != nullptr &&
                   busyCancellationObserved->load(std::memory_order_relaxed);
        }

        std::string sqliteErrorMessage(sqlite3* db, const std::string_view operation,
                                       const int rc) {
            return std::string{operation} + " failed: rc=" + std::to_string(rc) +
                   " extended_rc=" + std::to_string(sqlite3_extended_errcode(db)) +
                   " message=" + sqlite3_errmsg(db);
        }

        void executeReadTransactionCommand(sqlite3* db, const std::string_view command,
                                           const std::atomic_bool* busyCancellationObserved) {
            char* error = nullptr;
            const int rc = sqlite3_exec(db, std::string{command}.c_str(), nullptr, nullptr, &error);
            const std::string detail = error == nullptr ? sqlite3_errmsg(db) : error;
            sqlite3_free(error);
            if (isCanceledResult(rc, busyCancellationObserved)) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "sqlite read transaction canceled");
            }
            if (rc != SQLITE_OK) {
                throw ports::OperationError(
                    "Falha ao acessar o banco de dados",
                    sqliteErrorMessage(db, "sqlite read transaction " + std::string{command}, rc) +
                        " detail=" + detail);
            }
        }

    } // namespace

    SqliteConnection::SqliteConnection(const std::filesystem::path& dbPath,
                                       const SqliteOpenMode mode,
                                       const std::chrono::milliseconds busyTimeout) {
        const auto path = qt::toUtf8(dbPath);
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
        sqlite3_busy_timeout(db_, static_cast<int>(busyTimeout.count()));
    }

    SqliteConnection::~SqliteConnection() {
        closeHandle(db_);
    }

    sqlite3* SqliteConnection::handle() const noexcept {
        return db_;
    }

    SqliteStatement::SqliteStatement(sqlite3* db, const std::string& sql,
                                     const std::atomic_bool* busyCancellationObserved)
        : db_(db), busyCancellationObserved_(busyCancellationObserved) {
        const int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &statement_, nullptr);
        if (rc != SQLITE_OK) {
            if (isCanceledResult(rc, busyCancellationObserved_)) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "sqlite query canceled");
            }
            throw ports::OperationError("Falha ao acessar o banco de dados",
                                        sqliteErrorMessage(db_, "sqlite prepare", rc));
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
        if (isCanceledResult(rc, busyCancellationObserved_)) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite query canceled");
        }
        throw ports::OperationError("Falha ao acessar o banco de dados",
                                    sqliteErrorMessage(db_, "sqlite step", rc));
    }

    void SqliteStatement::executeAndReset() {
        const int rc = sqlite3_step(statement_);
        const auto stepError = rc == SQLITE_DONE || isCanceledResult(rc, busyCancellationObserved_)
                                   ? std::string{}
                                   : sqliteErrorMessage(db_, "sqlite statement execution", rc);
        hasCurrentRow_ = false;
        std::exception_ptr resetError;
        try {
            resetAndClearBindings();
        } catch (...) {
            resetError = std::current_exception();
        }
        if (isCanceledResult(rc, busyCancellationObserved_)) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite query canceled");
        }
        if (rc != SQLITE_DONE) {
            throw ports::OperationError("Falha ao acessar o banco de dados", stepError);
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
        if (text == nullptr) {
            return {};
        }
        const auto size = static_cast<std::size_t>(sqlite3_column_bytes(statement_, column));
        return std::string{text, size};
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

    SqliteReadTransaction::SqliteReadTransaction(sqlite3* db, std::stop_token stopToken,
                                                 const std::atomic_bool* busyCancellationObserved)
        : db_(db), stopToken_(std::move(stopToken)),
          busyCancellationObserved_(busyCancellationObserved) {
        if (db_ == nullptr) {
            throw std::invalid_argument("sqlite read transaction requires a connection");
        }
        throwIfCanceled(stopToken_);
        executeReadTransactionCommand(db_, "BEGIN", busyCancellationObserved_);
        if (sqlite3_get_autocommit(db_) != 0) {
            throw ports::OperationError("Falha ao acessar o banco de dados",
                                        "sqlite read transaction begin left autocommit enabled");
        }
        active_ = true;
    }

    SqliteReadTransaction::~SqliteReadTransaction() {
        if (!active_) {
            return;
        }
        char* error = nullptr;
        const int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &error);
        if (rc != SQLITE_OK) {
            sqlite3_log(rc, "sqlite read transaction rollback failed: %s",
                        error == nullptr ? sqlite3_errmsg(db_) : error);
        } else if (sqlite3_get_autocommit(db_) == 0) {
            sqlite3_log(SQLITE_ERROR, "sqlite read transaction rollback left transaction active");
        }
        sqlite3_free(error);
    }

    void SqliteReadTransaction::commit() {
        if (!active_) {
            throw std::logic_error("sqlite read transaction is not active");
        }
        throwIfCanceled(stopToken_);
        executeReadTransactionCommand(db_, "COMMIT", busyCancellationObserved_);
        if (sqlite3_get_autocommit(db_) == 0) {
            throw ports::OperationError("Falha ao acessar o banco de dados",
                                        "sqlite read transaction commit left transaction active");
        }
        active_ = false;
    }

    bool SqliteReadTransaction::active() const noexcept {
        return active_;
    }

    SqliteWriteTransaction::SqliteWriteTransaction(sqlite3* db,
                                                   const std::atomic_bool* busyCancellationObserved)
        : db_(db), busyCancellationObserved_(busyCancellationObserved) {
        if (db_ == nullptr) {
            throw std::invalid_argument("sqlite transaction requires a connection");
        }
        char* error = nullptr;
        const int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, &error);
        const std::string detail = error == nullptr ? sqlite3_errmsg(db_) : error;
        sqlite3_free(error);
        if (isCanceledResult(rc, busyCancellationObserved_)) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite transaction canceled");
        }
        if (rc != SQLITE_OK) {
            throw ports::OperationError("Falha ao iniciar a operacao no banco de dados",
                                        sqliteErrorMessage(db_, "sqlite transaction begin", rc) +
                                            " detail=" + detail);
        }
        if (sqlite3_get_autocommit(db_) != 0) {
            throw ports::OperationError("Falha ao iniciar a operacao no banco de dados",
                                        "sqlite transaction begin left autocommit enabled");
        }
        active_ = true;
    }

    SqliteWriteTransaction::~SqliteWriteTransaction() {
        if (!active_) {
            return;
        }
        char* error = nullptr;
        const int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &error);
        if (rc != SQLITE_OK) {
            sqlite3_log(rc, "sqlite transaction destructor rollback failed: %s",
                        error == nullptr ? sqlite3_errmsg(db_) : error);
        } else if (sqlite3_get_autocommit(db_) == 0) {
            sqlite3_log(SQLITE_ERROR,
                        "sqlite transaction destructor rollback left transaction active");
        }
        sqlite3_free(error);
    }

    void SqliteWriteTransaction::commit() {
        if (!active_) {
            throw std::logic_error("sqlite transaction is not active");
        }
        char* error = nullptr;
        const int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &error);
        const std::string detail = error == nullptr ? sqlite3_errmsg(db_) : error;
        sqlite3_free(error);
        if (sqlite3_get_autocommit(db_) != 0) {
            active_ = false;
        }
        if (isCanceledResult(rc, busyCancellationObserved_)) {
            throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                    "sqlite transaction commit canceled");
        }
        if (rc != SQLITE_OK) {
            throw ports::OperationError("Falha ao confirmar a operacao no banco de dados",
                                        sqliteErrorMessage(db_, "sqlite transaction commit", rc) +
                                            " detail=" + detail);
        }
        if (sqlite3_get_autocommit(db_) == 0) {
            throw ports::OperationError("Falha ao confirmar a operacao no banco de dados",
                                        "sqlite transaction commit left transaction active");
        }
        active_ = false;
    }

    void SqliteWriteTransaction::rollback() {
        if (!active_) {
            return;
        }
        char* error = nullptr;
        const int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &error);
        const std::string detail = error == nullptr ? sqlite3_errmsg(db_) : error;
        sqlite3_free(error);
        if (sqlite3_get_autocommit(db_) != 0) {
            active_ = false;
        }
        if (rc != SQLITE_OK) {
            throw ports::OperationError("Falha ao reverter a operacao no banco de dados",
                                        sqliteErrorMessage(db_, "sqlite transaction rollback", rc) +
                                            " detail=" + detail);
        }
        if (sqlite3_get_autocommit(db_) == 0) {
            throw ports::OperationError("Falha ao reverter a operacao no banco de dados",
                                        "sqlite transaction rollback left transaction active");
        }
        active_ = false;
    }

    bool SqliteWriteTransaction::active() const noexcept {
        return active_;
    }

} // namespace ssa::infra::sqlite
