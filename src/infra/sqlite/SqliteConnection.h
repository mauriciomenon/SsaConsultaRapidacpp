#pragma once

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string>

namespace ssa::infra::sqlite {

    enum class SqliteOpenMode {
        ReadOnly,
        ReadWrite,
        ReadWriteCreate,
    };

    class SqliteConnection final {
      public:
        explicit SqliteConnection(const std::filesystem::path& dbPath,
                                  SqliteOpenMode mode = SqliteOpenMode::ReadOnly,
                                  std::chrono::milliseconds busyTimeout = std::chrono::milliseconds{
                                      3000});
        ~SqliteConnection();

        SqliteConnection(const SqliteConnection&) = delete;
        SqliteConnection& operator=(const SqliteConnection&) = delete;
        SqliteConnection(SqliteConnection&&) = delete;
        SqliteConnection& operator=(SqliteConnection&&) = delete;

        [[nodiscard]] sqlite3* handle() const noexcept;

      private:
        sqlite3* db_{nullptr};
    };

    class SqliteStatement final {
      public:
        SqliteStatement(sqlite3* db, const std::string& sql,
                        const std::atomic_bool* busyCancellationObserved = nullptr);
        ~SqliteStatement();

        SqliteStatement(const SqliteStatement&) = delete;
        SqliteStatement& operator=(const SqliteStatement&) = delete;
        SqliteStatement(SqliteStatement&&) = delete;
        SqliteStatement& operator=(SqliteStatement&&) = delete;

        void bindTextOneBased(int index, const std::string& value);
        void bindNullOneBased(int index);
        void bindInt64OneBased(int index, long long value);
        [[nodiscard]] bool step();
        void executeAndReset();
        void resetAndClearBindings();
        [[nodiscard]] sqlite3_stmt* handle() const noexcept;
        [[nodiscard]] int columnCount() const;
        [[nodiscard]] std::string columnName(int column) const;
        [[nodiscard]] std::string columnText(int column) const;
        [[nodiscard]] long long columnInt64(int column) const;

      private:
        void requireCurrentRow() const;

        sqlite3* db_{nullptr};
        sqlite3_stmt* statement_{nullptr};
        const std::atomic_bool* busyCancellationObserved_{nullptr};
        bool hasCurrentRow_ = false;
    };

    class SqliteReadTransaction final {
      public:
        explicit SqliteReadTransaction(sqlite3* db, std::stop_token stopToken = {},
                                       const std::atomic_bool* busyCancellationObserved = nullptr);
        ~SqliteReadTransaction();

        SqliteReadTransaction(const SqliteReadTransaction&) = delete;
        SqliteReadTransaction& operator=(const SqliteReadTransaction&) = delete;
        SqliteReadTransaction(SqliteReadTransaction&&) = delete;
        SqliteReadTransaction& operator=(SqliteReadTransaction&&) = delete;

        void commit();
        [[nodiscard]] bool active() const noexcept;

      private:
        sqlite3* db_{nullptr};
        std::stop_token stopToken_;
        const std::atomic_bool* busyCancellationObserved_{nullptr};
        bool active_{false};
    };

    class SqliteWriteTransaction final {
      public:
        explicit SqliteWriteTransaction(sqlite3* db,
                                        const std::atomic_bool* busyCancellationObserved = nullptr);
        ~SqliteWriteTransaction();

        SqliteWriteTransaction(const SqliteWriteTransaction&) = delete;
        SqliteWriteTransaction& operator=(const SqliteWriteTransaction&) = delete;
        SqliteWriteTransaction(SqliteWriteTransaction&&) = delete;
        SqliteWriteTransaction& operator=(SqliteWriteTransaction&&) = delete;

        void commit();
        void rollback();
        [[nodiscard]] bool active() const noexcept;

      private:
        sqlite3* db_{nullptr};
        const std::atomic_bool* busyCancellationObserved_{nullptr};
        bool active_{false};
    };

} // namespace ssa::infra::sqlite
