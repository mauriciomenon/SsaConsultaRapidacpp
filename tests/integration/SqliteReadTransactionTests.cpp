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
