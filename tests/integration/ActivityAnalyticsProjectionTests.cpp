#include "infra/sqlite/SqliteActivityAnalyticsProjection.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"

#include "SqliteSsaImportWriterTestAccess.h"
#include "domain/ColumnCatalog.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

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

    sqlite3* openFixture(const std::filesystem::path& path) {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
        execute(db, "CREATE TABLE ssa_table("
                    "numero_ssa TEXT, situacao TEXT, setor_executor TEXT, "
                    "semana_cadastro INTEGER, solicitante TEXT, "
                    "responsavel_programacao TEXT, responsavel_execucao TEXT, "
                    "prazo_limite TEXT, status_execucao_prazo TEXT)");
        execute(db, "INSERT INTO ssa_table VALUES"
                    "('202600001','SPG','SMIN-A',202601,'Sol A','Plan A','Exec A',"
                    "'2026-02-05','Dentro do Prazo'),"
                    "('202600002','APG','SMIN-A',202552,'Sol B','Plan B','Exec B',"
                    "'2026-01-31','Fora de Prazo'),"
                    "('202600003','APL','SMIN-B',NULL,'Sol C','','Exec C',NULL,'Nao Se Aplica'),"
                    "('202600004','SES','SMIN-B',202602,'Sol D','Plan D','Exec D',"
                    "'2026-02-10','Dentro do Prazo'),"
                    "('202600005','ADM','SMIN-C',202603,'Sol E','Plan E','Exec E',"
                    "'2026-03-01','Dentro do Prazo')");
        return db;
    }

    ssa::infra::sqlite::ActivityAnalyticsCaptureContext
    captureContext(std::string revision = "revision-1", std::string fingerprint = "fingerprint-1") {
        return {.observedIsoYearWeek = 202605,
                .observedDate = "2026-02-01",
                .sourceRevision = std::move(revision),
                .sourceFingerprint = std::move(fingerprint)};
    }

} // namespace

TEST_CASE("activity analytics projection captures complete stock metrics atomically") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");

    const auto first = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());

    CHECK(first.changed);
    CHECK(first.snapshotMetrics == 6);
    CHECK(scalarInt(db, "SELECT schema_version FROM activity_analytics_meta WHERE dataset='SSA'") ==
          1);
    CHECK(scalarText(db, "SELECT active_source_revision FROM activity_analytics_meta "
                         "WHERE dataset='SSA'") == "revision-1");
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
    CHECK(scalarInt(db, "SELECT complete FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND metric='partial_attention'") == 0);
    CHECK_FALSE(scalarText(db, "SELECT reason FROM activity_analytics_snapshot "
                               "WHERE dataset='SSA' AND metric='partial_attention'")
                    .empty());
    CHECK(scalarInt(db, "SELECT SUM(count) FROM activity_analytics_point "
                        "WHERE metric='pending' AND person_role='executor'") == 4);
    CHECK(scalarInt(db, "SELECT SUM(count) FROM activity_analytics_point "
                        "WHERE metric='spg' AND person_role='executor'") == 1);
    CHECK(scalarInt(db, "SELECT deadline_offset_days FROM activity_analytics_point "
                        "WHERE metric='pending_deadline' AND person='Exec A'") == 4);

    const auto repeated = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());
    CHECK_FALSE(repeated.changed);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);

    auto changedDateContext = captureContext();
    changedDateContext.observedDate = "2026-01-30";
    const auto dateReplacement = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", changedDateContext);
    CHECK(dateReplacement.changed);
    CHECK(scalarText(db, "SELECT observed_date FROM activity_analytics_snapshot "
                         "WHERE dataset='SSA' AND metric='pending'") == "2026-01-30");
    CHECK(scalarInt(db, "SELECT deadline_offset_days FROM activity_analytics_point "
                        "WHERE metric='pending_deadline' AND person='Exec A'") == 6);

    execute(db, "UPDATE ssa_table SET situacao='SES' WHERE numero_ssa='202600001'");
    const auto replacement = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext("revision-2", "fingerprint-2"));
    CHECK(replacement.changed);
    CHECK(scalarText(db, "SELECT active_source_revision FROM activity_analytics_meta "
                         "WHERE dataset='SSA'") == "revision-2");
    CHECK(scalarInt(db, "SELECT SUM(count) FROM activity_analytics_point "
                        "WHERE metric='pending' AND person_role='executor'") == 3);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);

    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection rolls schema and points back with its caller") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");

    execute(db, "BEGIN IMMEDIATE");
    const auto result = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());
    CHECK(result.changed);
    execute(db, "ROLLBACK");

    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name LIKE 'activity_analytics_%'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection keeps latest metadata on out of order capture") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    const auto firstContext = captureContext();
    REQUIRE(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(db, "ssa_table",
                                                                           firstContext)
                .changed);
    auto latestContext = captureContext("revision-2", "fingerprint-2");
    latestContext.observedIsoYearWeek = 202606;
    latestContext.observedDate = "2026-02-08";
    REQUIRE(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(db, "ssa_table",
                                                                           latestContext)
                .changed);

    const auto repeatedHistorical = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", firstContext);

    CHECK_FALSE(repeatedHistorical.changed);
    CHECK(scalarText(db, "SELECT active_source_revision FROM activity_analytics_meta "
                         "WHERE dataset='SSA'") == "revision-2");
    CHECK(scalarInt(db, "SELECT baseline_iso_week FROM activity_analytics_meta "
                        "WHERE dataset='SSA'") == 202605);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA'") == 12);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection honors cancellation before mutation") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    std::stop_source stopSource;
    stopSource.request_stop();

    REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
                          db, "ssa_table", captureContext(), stopSource.get_token()),
                      std::system_error);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name LIKE 'activity_analytics_%'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection rejects invalid observed dates before mutation") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");

    for (const std::string invalidDate : {"2026-02-30", "2026-02-08"}) {
        auto context = captureContext();
        context.observedDate = invalidDate;
        REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
                              db, "ssa_table", context),
                          std::invalid_argument);
        CHECK(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                            "AND name LIKE 'activity_analytics_%'") == 0);
    }
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection fingerprints verified sources deterministically") {
    const auto first = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::sourceFingerprint(
        std::vector<std::string>{"source-b:20", "source-a:10"});
    const auto reordered = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::sourceFingerprint(
        std::vector<std::string>{"source-a:10", "source-b:20"});
    const auto changed = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::sourceFingerprint(
        std::vector<std::string>{"source-a:10", "source-b:21"});

    CHECK(first == reordered);
    CHECK(first != changed);
    CHECK(first.size() == 16);
}

TEST_CASE("activity analytics projection fingerprints canonical rows deterministically") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    execute(db, "ALTER TABLE ssa_table ADD COLUMN semana_executada INTEGER");
    execute(db, "ALTER TABLE ssa_table ADD COLUMN setor_emissor TEXT");

    const auto first =
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "ssa_table");
    execute(db, "CREATE TABLE reordered AS SELECT * FROM ssa_table ORDER BY numero_ssa DESC");
    const auto reordered =
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "reordered");
    execute(db, "UPDATE reordered SET setor_emissor='SMIN-Z' WHERE numero_ssa='202600001'");
    const auto changed =
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "reordered");

    CHECK(first == reordered);
    CHECK(first != changed);
    CHECK(first.size() == 16);

    std::stop_source stopSource;
    stopSource.request_stop();
    REQUIRE_THROWS_AS(
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "ssa_table", stopSource.get_token()),
        std::system_error);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection stores nonexistent registration ISO weeks as null") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    execute(db, "INSERT INTO ssa_table VALUES"
                "('202600006','SPG','SMIN-D',202153,'Sol F','Plan F','Exec Invalid',"
                "'2026-03-02','Dentro do Prazo')");

    const auto result = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());

    REQUIRE(result.changed);
    CHECK(scalarInt(db, "SELECT registration_iso_week IS NULL "
                        "FROM activity_analytics_point WHERE metric='pending' "
                        "AND person_role='executor' AND person='Exec Invalid'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection rejects noncanonical registration week storage") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    execute(db, "INSERT INTO ssa_table VALUES"
                "('202600006','SPG','SMIN-D','202052junk','Sol F','Plan F','Exec Junk',"
                "'2026-03-02','Dentro do Prazo'),"
                "('202600007','SPG','SMIN-D',202052.5,'Sol G','Plan G','Exec Real',"
                "'2026-03-02','Dentro do Prazo')");

    const auto result = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());

    REQUIRE(result.changed);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_point "
                        "WHERE metric='pending' AND person_role='executor' "
                        "AND person IN ('Exec Junk', 'Exec Real') "
                        "AND registration_iso_week IS NULL") == 2);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection replaces a captured week with an extra metric") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    REQUIRE(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(db, "ssa_table",
                                                                           captureContext())
                .changed);
    execute(db, "INSERT INTO activity_analytics_snapshot(dataset, observed_iso_week, metric, "
                "source_revision, source_fingerprint, observed_date, complete, reason) "
                "VALUES('SSA', 202605, 'legacy_metric', 'revision-1', 'fingerprint-1', "
                "'2026-02-01', 0, 'legacy')");

    const auto repaired = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
        db, "ssa_table", captureContext());

    CHECK(repaired.changed);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND observed_iso_week=202605") == 6);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE dataset='SSA' AND metric='legacy_metric'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("canonical analytics fingerprint is stable with duplicate SSA numbers") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");
    execute(db, "ALTER TABLE ssa_table ADD COLUMN semana_executada INTEGER");
    execute(db, "ALTER TABLE ssa_table ADD COLUMN setor_emissor TEXT");
    execute(db, "INSERT INTO ssa_table(numero_ssa, situacao, setor_executor, semana_cadastro, "
                "solicitante, responsavel_programacao, responsavel_execucao, prazo_limite, "
                "status_execucao_prazo, semana_executada, setor_emissor) VALUES"
                "('202600001','APG','SMIN-Z',202604,'Sol Z','Plan Z','Exec Z',"
                "'2026-02-20','Dentro do Prazo',202605,'SMIN')");
    execute(db, "CREATE TABLE reversed AS SELECT * FROM ssa_table ORDER BY rowid DESC");

    const auto original =
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "ssa_table");
    const auto reversed =
        ssa::infra::sqlite::SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            db, "reversed");

    CHECK(original == reversed);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("activity analytics projection rejects unsafe table identifiers") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    sqlite3* db = openFixture(std::filesystem::path{directory.path().toStdString()} / "ssas.db");

    REQUIRE_THROWS_AS(ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
                          db, "ssa_table; DROP TABLE ssa_table", captureContext()),
                      std::invalid_argument);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 5);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import publishes canonical rows and analytics in one finish") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto root = std::filesystem::path{directory.path().toStdString()};
    const auto dbPath = root / "ssas.db";
    const auto source = root / "source.xlsx";
    const auto destination = root / "processed" / "source.xlsx";
    {
        std::ofstream file(source);
        REQUIRE(file.is_open());
        file << "verified source";
    }

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(
        ssa::infra::sqlite::SqliteSsaImportWriterTestAccess::access(), dbPath,
        ssa::domain::ColumnCatalog::schemaColumns());
    auto session = writer.startSession(true);
    ssa::infra::importing::ResolvedSsaImportRows rows;
    rows.rows.push_back({{"numero_ssa", "202600101"},
                         {"situacao", "SPG"},
                         {"setor_executor", "SMIN-A"},
                         {"semana_cadastro", "202601"},
                         {"responsavel_execucao", "Exec A"},
                         {"prazo_limite", "2026-02-05"},
                         {"status_execucao_prazo", "Dentro do Prazo"}});
    CHECK(session.write(rows, 1, 0).rowsWritten == 1);
    session.recordConsolidation({{{source, destination, true}}});

    const auto summary = session.finishWithAnalytics(202605, "2026-02-01");

    CHECK(summary.rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600101'") == 1);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot "
                        "WHERE observed_iso_week=202605") == 6);
    CHECK(scalarText(db, "SELECT source_revision FROM activity_analytics_snapshot "
                         "WHERE metric='pending'") ==
          scalarText(db, "SELECT source_fingerprint FROM activity_analytics_snapshot "
                         "WHERE metric='pending'"));
    CHECK(scalarText(db, "SELECT source_revision FROM activity_analytics_snapshot "
                         "WHERE metric='pending'")
              .size() == 16);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import fingerprints canonical rows without consolidation moves") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto dbPath = std::filesystem::path{directory.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(
        ssa::infra::sqlite::SqliteSsaImportWriterTestAccess::access(), dbPath,
        ssa::domain::ColumnCatalog::schemaColumns());
    auto session = writer.startSession(true);
    ssa::infra::importing::ResolvedSsaImportRows rows;
    rows.rows.push_back({{"numero_ssa", "202600102"}, {"situacao", "SPG"}});
    CHECK(session.write(rows, 1, 0).rowsWritten == 1);

    const auto summary = session.finishWithAnalytics(202605, "2026-02-01");

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    CHECK(summary.rowsWritten == 1);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600102'") == 1);
    CHECK(scalarInt(db, "SELECT COUNT(*) FROM activity_analytics_snapshot") == 6);
    CHECK(scalarText(db, "SELECT source_fingerprint FROM activity_analytics_snapshot LIMIT 1")
              .size() == 16);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}
