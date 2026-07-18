#include "infra/sqlite/SqliteConnection.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stop_token>
#include <system_error>

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
