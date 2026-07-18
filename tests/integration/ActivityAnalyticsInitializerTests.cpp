#include "infra/sqlite/SqliteActivityAnalyticsInitializer.h"

#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "ports/OperationError.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

    void execute(sqlite3* db, const std::string& sql) {
        char* error = nullptr;
        const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
        const std::string detail = error == nullptr ? std::string{} : std::string{error};
        sqlite3_free(error);
        if (result != SQLITE_OK) {
            throw std::runtime_error(detail);
        }
    }

    long long scalarInt(sqlite3* db, const std::string& sql) {
        sqlite3_stmt* statement = nullptr;
        REQUIRE(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
        const auto value = sqlite3_column_int64(statement, 0);
        REQUIRE(sqlite3_finalize(statement) == SQLITE_OK);
        return value;
    }

    std::string scalarText(sqlite3* db, const std::string& sql) {
        sqlite3_stmt* statement = nullptr;
        REQUIRE(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const std::string value = text == nullptr ? std::string{} : std::string{text};
        REQUIRE(sqlite3_finalize(statement) == SQLITE_OK);
        return value;
    }

    std::filesystem::path createFixture(const QTemporaryDir& directory, const std::string& name) {
        const auto path = std::filesystem::path{directory.path().toStdString()} / name;
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
        execute(db, "CREATE TABLE ssa_table("
                    "numero_ssa TEXT, situacao TEXT, setor_executor TEXT, "
                    "setor_emissor TEXT, semana_cadastro INTEGER, semana_executada INTEGER, "
                    "solicitante TEXT, "
                    "responsavel_programacao TEXT, responsavel_execucao TEXT, "
                    "prazo_limite TEXT, status_execucao_prazo TEXT)");
        execute(db, "INSERT INTO ssa_table VALUES"
                    "('202600001','SPG','SMIN-A','SMIN',202601,202602,'Sol A','Plan A','Exec A',"
                    "'2026-02-05','Dentro do Prazo'),"
                    "('202600002','APG','SMIN-B','SMIN',202552,202553,'Sol B','Plan B','Exec B',"
                    "'2026-01-31','Fora de Prazo')");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
        return path;
    }

    sqlite3* openReadWrite(const std::filesystem::path& path) {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
        return db;
    }

} // namespace

TEST_CASE("activity analytics initializer creates one baseline snapshot") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createFixture(directory, "ssas.db");

    const auto first = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
        path, 202605, "2026-02-01");

    CHECK(first.changed);
    CHECK(first.snapshotMetrics == 6);
    sqlite3* db = openReadWrite(path);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
    const auto fingerprint = scalarText(
        db, "SELECT source_fingerprint FROM activity_analytics_snapshot WHERE metric='pending'");
    CHECK(fingerprint.size() == 16);
    CHECK(scalarText(db, "SELECT source_revision FROM activity_analytics_snapshot "
                         "WHERE metric='pending'") == "baseline-" + fingerprint);
    execute(db, "INSERT INTO ssa_table VALUES"
                "('202600003','APL','SMIN-C','SMIN',202602,202603,'Sol C','Plan C','Exec C',"
                "'2026-02-07','Dentro do Prazo')");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    const auto repeated = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
        path, 202606, "2026-02-08");

    CHECK_FALSE(repeated.changed);
    CHECK(repeated.snapshotMetrics == 0);
    db = openReadWrite(path);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot WHERE dataset='SSA'") ==
          6);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202606") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics initializer derives deterministic source identity") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto firstPath = createFixture(directory, "first.db");
    const auto secondPath = createFixture(directory, "second.db");
    const auto changedPath = createFixture(directory, "changed.db");
    sqlite3* changed = openReadWrite(changedPath);
    execute(changed, "UPDATE ssa_table SET situacao=CASE numero_ssa "
                     "WHEN '202600001' THEN 'APG' ELSE 'SPG' END, "
                     "setor_executor=CASE numero_ssa WHEN '202600001' THEN 'SMIN-B' "
                     "ELSE 'SMIN-A' END");
    REQUIRE(sqlite3_close(changed) == SQLITE_OK);

    CHECK(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(firstPath, 202605,
                                                                             "2026-02-01")
              .changed);
    CHECK(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(secondPath, 202605,
                                                                             "2026-02-01")
              .changed);
    CHECK(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(changedPath, 202605,
                                                                             "2026-02-01")
              .changed);

    sqlite3* first = openReadWrite(firstPath);
    sqlite3* second = openReadWrite(secondPath);
    const auto firstFingerprint =
        scalarText(first, "SELECT source_fingerprint FROM activity_analytics_snapshot LIMIT 1");
    const auto secondFingerprint =
        scalarText(second, "SELECT source_fingerprint FROM activity_analytics_snapshot LIMIT 1");
    changed = openReadWrite(changedPath);
    const auto changedFingerprint =
        scalarText(changed, "SELECT source_fingerprint FROM activity_analytics_snapshot LIMIT 1");
    CHECK(firstFingerprint == secondFingerprint);
    CHECK(firstFingerprint != changedFingerprint);
    REQUIRE(sqlite3_close(first) == SQLITE_OK);
    REQUIRE(sqlite3_close(second) == SQLITE_OK);
    REQUIRE(sqlite3_close(changed) == SQLITE_OK);
}

TEST_CASE("activity analytics initializer validates metadata and the complete metric set") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createFixture(directory, "ssas.db");
    REQUIRE(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(path, 202605,
                                                                               "2026-02-01")
                .changed);

    SECTION("schema version") {
        sqlite3* db = openReadWrite(path);
        execute(db, "UPDATE activity_analytics_meta SET schema_version=0 WHERE dataset='SSA'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
            path, 202605, "2026-02-01");

        CHECK(repaired.changed);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT schema_version FROM activity_analytics_meta "
                            "WHERE dataset='SSA'") == 1);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("metadata identity and baseline") {
        sqlite3* db = openReadWrite(path);
        execute(db, "UPDATE activity_analytics_meta SET active_source_revision='', "
                    "baseline_iso_week=202604 WHERE dataset='SSA'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
            path, 202605, "2026-02-01");

        CHECK(repaired.changed);
        db = openReadWrite(path);
        CHECK(scalarText(db, "SELECT active_source_revision FROM activity_analytics_meta "
                             "WHERE dataset='SSA'") ==
              scalarText(db, "SELECT source_revision FROM activity_analytics_snapshot "
                             "WHERE dataset='SSA' AND observed_iso_week=202605 LIMIT 1"));
        CHECK(scalarInt(db, "SELECT baseline_iso_week FROM activity_analytics_meta "
                            "WHERE dataset='SSA'") == 202605);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("metadata numeric storage classes") {
        sqlite3* db = openReadWrite(path);
        execute(db, "UPDATE activity_analytics_meta SET schema_version='1junk', "
                    "baseline_iso_week='202605junk' WHERE dataset='SSA'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
            path, 202605, "2026-02-01");

        CHECK(repaired.changed);
        db = openReadWrite(path);
        CHECK(scalarText(db, "SELECT TYPEOF(schema_version) FROM activity_analytics_meta "
                             "WHERE dataset='SSA'") == "integer");
        CHECK(scalarText(db, "SELECT TYPEOF(baseline_iso_week) FROM activity_analytics_meta "
                             "WHERE dataset='SSA'") == "integer");
        CHECK(scalarInt(db, "SELECT schema_version FROM activity_analytics_meta "
                            "WHERE dataset='SSA'") == 1);
        CHECK(scalarInt(db, "SELECT baseline_iso_week FROM activity_analytics_meta "
                            "WHERE dataset='SSA'") == 202605);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("snapshot metric set") {
        sqlite3* db = openReadWrite(path);
        execute(db, "UPDATE activity_analytics_snapshot SET metric='legacy_metric' "
                    "WHERE dataset='SSA' AND metric='pending_deadline'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
            path, 202605, "2026-02-01");

        CHECK(repaired.changed);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND metric='legacy_metric'") == 0);
        CHECK(scalarInt(db, "SELECT COUNT(DISTINCT metric) "
                            "FROM activity_analytics_snapshot WHERE dataset='SSA' AND metric IN "
                            "('partial_attention','spg','apg','apl','pending',"
                            "'pending_deadline')") == 6);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("snapshot week storage class") {
        sqlite3* db = openReadWrite(path);
        execute(db, "UPDATE activity_analytics_snapshot "
                    "SET observed_iso_week='202605junk' WHERE dataset='SSA'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                              path, 202605, "2026-02-01"),
                          ssa::ports::OperationError);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND TYPEOF(observed_iso_week)='text'") == 6);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA'") == 6);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("missing point table") {
        sqlite3* db = openReadWrite(path);
        execute(db, "DROP TABLE activity_analytics_point");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
            path, 202605, "2026-02-01");

        CHECK(repaired.changed);
        db = openReadWrite(path);
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                              "AND name='activity_analytics_point'") == 1);
        CHECK(scalarInt(db, "SELECT SUM(count) FROM activity_analytics_point "
                            "WHERE dataset='SSA' AND metric='pending'") > 0);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("incompatible point table") {
        sqlite3* db = openReadWrite(path);
        execute(db, "DROP TABLE activity_analytics_point");
        execute(db, "CREATE TABLE activity_analytics_point(id INTEGER PRIMARY KEY)");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                              path, 202605, "2026-02-01"),
                          ssa::ports::OperationError);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM pragma_table_info('activity_analytics_point')") ==
              1);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("orphan point") {
        sqlite3* db = openReadWrite(path);
        execute(db, "INSERT INTO activity_analytics_point("
                    "dataset, observed_iso_week, metric, division, sector, person_role, person, "
                    "registration_iso_week, deadline_source_state, deadline_offset_days, count) "
                    "VALUES('SSA',202605,'legacy_metric','DIV','SEC','executor','Person',NULL,'',"
                    "NULL,1)");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                              path, 202605, "2026-02-01"),
                          ssa::ports::OperationError);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_point "
                            "WHERE dataset='SSA' AND metric='legacy_metric'") == 1);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("duplicate point group") {
        sqlite3* db = openReadWrite(path);
        execute(db, "INSERT INTO activity_analytics_point("
                    "dataset, observed_iso_week, metric, division, sector, person_role, person, "
                    "registration_iso_week, deadline_source_state, deadline_offset_days, count) "
                    "SELECT dataset, observed_iso_week, metric, division, sector, person_role, "
                    "person, registration_iso_week, deadline_source_state, deadline_offset_days, "
                    "count FROM activity_analytics_point WHERE dataset='SSA' LIMIT 1");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                              path, 202605, "2026-02-01"),
                          ssa::ports::OperationError);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_point WHERE dataset='SSA'") >
              0);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }

    SECTION("corrupt historical metric set") {
        sqlite3* db = openReadWrite(path);
        execute(db, "INSERT INTO activity_analytics_snapshot(dataset, observed_iso_week, metric, "
                    "source_revision, source_fingerprint, observed_date, complete, reason) "
                    "SELECT dataset, 202606, metric, source_revision, source_fingerprint, "
                    "'2026-02-08', complete, reason FROM activity_analytics_snapshot "
                    "WHERE dataset='SSA' AND observed_iso_week=202605");
        execute(db, "UPDATE activity_analytics_snapshot SET metric='legacy_metric' "
                    "WHERE dataset='SSA' AND observed_iso_week=202606 "
                    "AND metric='pending_deadline'");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                              path, 202605, "2026-02-01"),
                          ssa::ports::OperationError);
        db = openReadWrite(path);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND observed_iso_week=202606") == 6);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                            "WHERE dataset='SSA' AND metric='legacy_metric'") == 1);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);
    }
}

TEST_CASE("activity analytics initializer honors cancellation before mutation") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createFixture(directory, "ssas.db");
    std::stop_source stopSource;
    stopSource.request_stop();

    REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                          path, 202605, "2026-02-01", stopSource.get_token()),
                      std::system_error);

    sqlite3* db = openReadWrite(path);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name LIKE 'activity_analytics_%'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics initializer rolls projection failure back") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createFixture(directory, "ssas.db");
    sqlite3* db = openReadWrite(path);
    execute(db, "CREATE TABLE activity_analytics_point(id INTEGER PRIMARY KEY)");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                          path, 202605, "2026-02-01"),
                      ssa::ports::OperationError);

    db = openReadWrite(path);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='activity_analytics_point'") == 1);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='activity_analytics_meta'") == 0);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='activity_analytics_snapshot'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics initializer requires the database write lock") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createFixture(directory, "ssas.db");
    const ssa::infra::sqlite::SqliteDatabaseWriteLock heldLock(path);
    REQUIRE(heldLock.acquired());

    REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsInitializer::initialize(
                          path, 202605, "2026-02-01"),
                      ssa::ports::OperationError);

    sqlite3* db = openReadWrite(path);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name LIKE 'activity_analytics_%'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}
