#include "infra/sqlite/SqliteConnection.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <atomic>
#include <exception>
#include <filesystem>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

    std::filesystem::path createDatabase(const QTemporaryDir& directory) {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "query.sqlite";
        const ssa::infra::sqlite::SqliteConnection writer(
            path, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);
        REQUIRE(sqlite3_exec(writer.handle(),
                             "PRAGMA journal_mode=DELETE;"
                             "CREATE TABLE state(value TEXT NOT NULL);"
                             "INSERT INTO state VALUES('kept');",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        return path;
    }

} // namespace

TEST_CASE("sqlite query-only connection reads and rejects writes") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createDatabase(directory);
    const ssa::infra::sqlite::SqliteConnection reader(path);

    ssa::infra::sqlite::SqliteStatement value(reader.handle(), "SELECT value FROM state");
    REQUIRE(value.step());
    REQUIRE(value.columnText(0) == "kept");
    REQUIRE(sqlite3_exec(reader.handle(), "INSERT INTO state VALUES('lost')", nullptr, nullptr,
                         nullptr) == SQLITE_READONLY);
}

TEST_CASE("sqlite query-only connection opens a write-protected file") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createDatabase(directory);
    std::filesystem::permissions(path, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    const ssa::infra::sqlite::SqliteConnection reader(path);
    ssa::infra::sqlite::SqliteStatement value(reader.handle(), "SELECT value FROM state");
    REQUIRE(value.step());
    REQUIRE(value.columnText(0) == "kept");
    std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
}

TEST_CASE("sqlite runtime library matches the compiled header") {
    CHECK(std::string_view{sqlite3_libversion()} == SQLITE_VERSION);
    CHECK(std::string_view{sqlite3_sourceid()} == SQLITE_SOURCE_ID);
}

// Regression: the macOS system SQLite failed BEGIN IMMEDIATE with SQLITE_IOERR_LOCK (EBADF)
// in about one of three attempts while read-only connections opened and closed the same file.
// ReadOnly readers (database validation, backup source) prove the linked library is not that one.
TEST_CASE("sqlite write transaction begins while readers open and close") {
    const auto readerMode = GENERATE(ssa::infra::sqlite::SqliteOpenMode::QueryOnly,
                                     ssa::infra::sqlite::SqliteOpenMode::ReadOnly);
    CAPTURE(static_cast<int>(readerMode));
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createDatabase(directory);
    std::atomic_bool stop{false};
    std::atomic_int readerFailures{0};
    std::vector<std::jthread> readers;
    readers.reserve(4);
    for (int index = 0; index < 4; ++index) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                try {
                    const ssa::infra::sqlite::SqliteConnection reader(path, readerMode);
                    ssa::infra::sqlite::SqliteStatement count(reader.handle(),
                                                              "SELECT COUNT(*) FROM state");
                    if (!count.step()) {
                        readerFailures.fetch_add(1);
                    }
                } catch (const std::exception&) {
                    readerFailures.fetch_add(1);
                }
            }
        });
    }

    int writeFailures = 0;
    for (int attempt = 0; attempt < 300; ++attempt) {
        try {
            const ssa::infra::sqlite::SqliteConnection writer(
                path, ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
            ssa::infra::sqlite::SqliteWriteTransaction transaction(writer.handle());
            transaction.commit();
        } catch (const std::exception& error) {
            if (writeFailures == 0) {
                UNSCOPED_INFO("first write failure: " << error.what());
            }
            ++writeFailures;
        }
    }
    stop.store(true);
    readers.clear();

    CHECK(readerFailures.load() == 0);
    CHECK(writeFailures == 0);
}

TEST_CASE("sqlite read transaction commits and restores autocommit") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = std::filesystem::path{directory.path().toStdString()} / "read.sqlite";
    ssa::infra::sqlite::SqliteConnection connection(
        path, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);

    ssa::infra::sqlite::SqliteReadTransaction transaction(connection.handle());
    CHECK(transaction.active());
    CHECK(sqlite3_get_autocommit(connection.handle()) == 0);

    transaction.commit();
    CHECK_FALSE(transaction.active());
    CHECK(sqlite3_get_autocommit(connection.handle()) != 0);
    CHECK_THROWS_AS(transaction.commit(), std::logic_error);
}

TEST_CASE("sqlite read transaction rolls back on destruction") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = std::filesystem::path{directory.path().toStdString()} / "read.sqlite";
    ssa::infra::sqlite::SqliteConnection connection(
        path, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);

    {
        const ssa::infra::sqlite::SqliteReadTransaction transaction(connection.handle());
        CHECK(transaction.active());
        CHECK(sqlite3_get_autocommit(connection.handle()) == 0);
    }

    CHECK(sqlite3_get_autocommit(connection.handle()) != 0);
}

TEST_CASE("sqlite read transaction honors cancellation before begin") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = std::filesystem::path{directory.path().toStdString()} / "read.sqlite";
    ssa::infra::sqlite::SqliteConnection connection(
        path, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);
    std::stop_source stopSource;
    stopSource.request_stop();

    CHECK_THROWS_AS(
        ssa::infra::sqlite::SqliteReadTransaction(connection.handle(), stopSource.get_token()),
        std::system_error);
    CHECK(sqlite3_get_autocommit(connection.handle()) != 0);
}

TEST_CASE("sqlite read transaction keeps one WAL snapshot until commit") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = std::filesystem::path{directory.path().toStdString()} / "snapshot.sqlite";
    ssa::infra::sqlite::SqliteConnection writer(
        path, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);
    REQUIRE(sqlite3_exec(writer.handle(),
                         "PRAGMA journal_mode=WAL;"
                         "CREATE TABLE state(value TEXT NOT NULL);"
                         "INSERT INTO state VALUES('before');"
                         "PRAGMA user_version=1;",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    ssa::infra::sqlite::SqliteConnection reader(path, ssa::infra::sqlite::SqliteOpenMode::ReadOnly);

    ssa::infra::sqlite::SqliteReadTransaction transaction(reader.handle());
    {
        ssa::infra::sqlite::SqliteStatement version(reader.handle(), "PRAGMA user_version");
        REQUIRE(version.step());
        REQUIRE(version.columnInt64(0) == 1);
    }
    REQUIRE(sqlite3_exec(writer.handle(),
                         "BEGIN IMMEDIATE;"
                         "UPDATE state SET value='after';"
                         "PRAGMA user_version=2;"
                         "COMMIT;",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    {
        ssa::infra::sqlite::SqliteStatement version(reader.handle(), "PRAGMA user_version");
        REQUIRE(version.step());
        REQUIRE(version.columnInt64(0) == 1);
        ssa::infra::sqlite::SqliteStatement value(reader.handle(), "SELECT value FROM state");
        REQUIRE(value.step());
        REQUIRE(value.columnText(0) == "before");
    }

    transaction.commit();
    ssa::infra::sqlite::SqliteStatement version(reader.handle(), "PRAGMA user_version");
    REQUIRE(version.step());
    REQUIRE(version.columnInt64(0) == 2);
    ssa::infra::sqlite::SqliteStatement value(reader.handle(), "SELECT value FROM state");
    REQUIRE(value.step());
    REQUIRE(value.columnText(0) == "after");
}
