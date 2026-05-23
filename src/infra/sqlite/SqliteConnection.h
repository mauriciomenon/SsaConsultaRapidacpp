#pragma once

#include <sqlite3.h>

#include <filesystem>
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
                                  int busyTimeoutMs = 3000);
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
        SqliteStatement(sqlite3* db, const std::string& sql);
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

        sqlite3_stmt* statement_{nullptr};
        bool hasCurrentRow_{false};
    };

} // namespace ssa::infra::sqlite
