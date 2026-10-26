#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivationGraphReader.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace {

    namespace derivation = ssa::domain::derivation;
    using ssa::infra::sqlite::SqliteConnection;
    using ssa::infra::sqlite::SqliteOpenMode;
    using ssa::infra::sqlite::SqliteStatement;

    void exec(sqlite3* db, const char* sql) {
        char* error = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
        const std::string message = error != nullptr ? error : "";
        sqlite3_free(error);
        INFO(message);
        REQUIRE(rc == SQLITE_OK);
    }

    // Subset of the Python schema (armazenamento/derivadas_schema.py) the reader touches.
    constexpr auto kSyncSchema = R"SQL(
        CREATE TABLE ssa_table(numero_ssa TEXT, derivada_de TEXT);
        CREATE TABLE ssa_derivada_source(parent_ssa TEXT, child_ssa TEXT, source_name TEXT,
            source_flag INTEGER, relation_type INTEGER, relation_raw_label TEXT, is_active INTEGER);
        CREATE TABLE ssa_derivada_matrix(parent_ssa TEXT, child_ssa TEXT, source_flags INTEGER,
            relation_type INTEGER, relation_raw_label TEXT, active INTEGER);
        CREATE TABLE ssa_derivada_sync_run(sync_run_id INTEGER PRIMARY KEY, status TEXT,
            graph_fingerprint TEXT);
    )SQL";

    // The golden graph of DerivationGraphTests, as the Python sync stores it.
    constexpr auto kGoldenMatrix = R"SQL(
        INSERT INTO ssa_derivada_matrix VALUES
            ('202500009','202500007',1,1,NULL,1), ('202500001','202500002',3,2,'Derivada da',1),
            ('202500001','202500003',1,1,NULL,1), ('202500002','202500004',1,1,NULL,1),
            ('202500003','202500004',2,2,'Complementa "a"\',1),
            ('202500004','202500005',2,2,'Consequ' || char(234) || 'ncia a',1),
            ('202500006','202500001',2,2,NULL,1), ('202500007','202500008',1,1,NULL,1),
            ('202500008','202500009',1,1,NULL,1), ('202500001','202500099',1,1,NULL,0);
        INSERT INTO ssa_derivada_sync_run VALUES (1,'ok','v2:old'), (2,'ok','v2:last'),
            (3,'error','v2:failed');
    )SQL";

    std::filesystem::path createDatabase(const QTemporaryDir& directory, const char* schema) {
        const auto path = std::filesystem::path{directory.path().toStdString()} / "graph.sqlite";
        const SqliteConnection connection(path, SqliteOpenMode::ReadWriteCreate);
        exec(connection.handle(), schema);
        return path;
    }

} // namespace

TEST_CASE("derivation reader matches the Python v2 fingerprint of the stored matrix") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createDatabase(directory, kSyncSchema);
    {
        const SqliteConnection writer(path, SqliteOpenMode::ReadWrite);
        exec(writer.handle(), kGoldenMatrix);
    }
    const SqliteConnection reader(path);
    const auto snapshot = ssa::infra::sqlite::readDerivationGraph(reader.handle());

    REQUIRE(snapshot.syncTablesPresent);
    REQUIRE(snapshot.storedMatrix.size() == 9);
    CHECK(snapshot.lastOkFingerprint == std::optional<std::string>{"v2:last"});
    // Python _graph_fingerprint of the golden graph (see DerivationGraphTests).
    CHECK(ssa::infra::sqlite::derivationGraphFingerprint(snapshot.storedMatrix) ==
          "v2:d108f14bcfd7a05bda54d1cab256b28ddfd66311754a83e6c9f222b2882737da");
    // Python _graph_fingerprint(..., legacy=True), the format of runs before v2.
    // pragma: allowlist nextline secret
    constexpr auto kLegacy = "7fc677e796c36471ec8634030b5a5368a22e5d104b7172e48c1c5a05fe18612b";
    CHECK(ssa::infra::sqlite::derivationGraphFingerprint(
              snapshot.storedMatrix, derivation::FingerprintFormat::Legacy) == kLegacy);
    CHECK(ssa::infra::sqlite::derivationGraphFingerprintMatches(snapshot.storedMatrix, kLegacy));
    CHECK_FALSE(ssa::infra::sqlite::derivationGraphFingerprintMatches(
        snapshot.storedMatrix, "v2:" + std::string{kLegacy}));
}

TEST_CASE("derivation reader collects db field edges like the Python collect_db_edges") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = createDatabase(directory, "CREATE TABLE ssa_table(numero_ssa TEXT, "
                                                "derivada_de TEXT);");
    {
        const SqliteConnection writer(path, SqliteOpenMode::ReadWrite);
        exec(writer.handle(), "INSERT INTO ssa_table VALUES "
                              "('202500002','202500001'), ('202500003',' 202500001 '),"
                              "('202500004','202500004'), ('202500005','abc'),"
                              "('202500006',NULL), ('202500007','202500001.0'),"
                              "('202500002','202500001');");
    }
    const SqliteConnection reader(path);
    const auto snapshot = ssa::infra::sqlite::readDerivationGraph(reader.handle());

    CHECK_FALSE(snapshot.syncTablesPresent);
    std::vector<std::tuple<std::string, std::string>> pairs;
    for (const auto& edge : snapshot.dbFieldEdges) {
        CHECK(edge.sourceName == derivation::kSourceDbField);
        CHECK(edge.relationType == derivation::kRelationDbField);
        pairs.emplace_back(edge.parent, edge.child);
    }
    CHECK(pairs == std::vector<std::tuple<std::string, std::string>>{{"202500001", "202500002"},
                                                                     {"202500001", "202500003"},
                                                                     {"202500001", "202500007"}});
    CHECK(snapshot.knownSsas.size() == 6);
}

// Opt-in equivalence against a copy of a database last synced by the Python app:
// SSA_DERIVADAS_EQUIVALENCE_DB=<copy>. Never point it at the operational database.
TEST_CASE("derivation engine reproduces the Python sync tables of a real database") {
    const char* databasePath = std::getenv("SSA_DERIVADAS_EQUIVALENCE_DB");
    if (databasePath == nullptr || *databasePath == '\0') {
        SKIP("SSA_DERIVADAS_EQUIVALENCE_DB is not set");
    }
    const SqliteConnection connection(databasePath, SqliteOpenMode::ReadOnly);
    sqlite3* db = connection.handle();
    const auto snapshot = ssa::infra::sqlite::readDerivationGraph(db);
    REQUIRE(snapshot.syncTablesPresent);
    REQUIRE(snapshot.lastOkFingerprint.has_value());

    CHECK(ssa::infra::sqlite::derivationGraphFingerprintMatches(snapshot.storedMatrix,
                                                                *snapshot.lastOkFingerprint));

    // The persisted matrix is the merge of the active sources (labels are sticky in the
    // Python upsert, so only flags and relation types must agree).
    const auto merged = derivation::mergeSources(snapshot.storedSources);
    REQUIRE(merged.size() == snapshot.storedMatrix.size());
    std::size_t labelDifferences = 0;
    for (std::size_t index = 0; index < merged.size(); ++index) {
        const auto& left = merged[index];
        const auto& right = snapshot.storedMatrix[index];
        REQUIRE(std::tie(left.parent, left.child, left.sourceFlags, left.relationType) ==
                std::tie(right.parent, right.child, right.sourceFlags, right.relationType));
        labelDifferences += left.label == right.label ? 0 : 1;
    }
    UNSCOPED_INFO("label differences between merged sources and matrix: " << labelDifferences);

    const auto closure = derivation::buildClosure(snapshot.storedMatrix);
    std::vector<derivation::ClosureRow> storedClosure;
    SqliteStatement closureRows(db, "SELECT ancestor_ssa, descendant_ssa, min_distance, "
                                    "max_distance, path_count FROM ssa_derivada_closure "
                                    "ORDER BY ancestor_ssa, descendant_ssa");
    while (closureRows.step()) {
        storedClosure.push_back({closureRows.columnText(0), closureRows.columnText(1),
                                 static_cast<int>(closureRows.columnInt64(2)),
                                 static_cast<int>(closureRows.columnInt64(3)),
                                 closureRows.columnInt64(4)});
    }
    CHECK(closure.rows.size() == storedClosure.size());
    CHECK(closure.rows == storedClosure);

    const auto summary = derivation::buildSummary(snapshot.storedMatrix, closure);
    std::vector<derivation::SummaryRow> storedSummary;
    SqliteStatement summaryRows(
        db, "SELECT ssa, direct_parents_count, direct_children_count, ancestors_count, "
            "descendants_count, level_from_root_min, level_from_root_max, levels_above_max, "
            "levels_below_max, component_size, has_cycle FROM ssa_derivada_summary ORDER BY ssa");
    const auto optionalInt = [&summaryRows](const int column) -> std::optional<int> {
        if (sqlite3_column_type(summaryRows.handle(), column) == SQLITE_NULL) {
            return std::nullopt;
        }
        return static_cast<int>(summaryRows.columnInt64(column));
    };
    while (summaryRows.step()) {
        storedSummary.push_back(
            {summaryRows.columnText(0), static_cast<int>(summaryRows.columnInt64(1)),
             static_cast<int>(summaryRows.columnInt64(2)),
             static_cast<int>(summaryRows.columnInt64(3)),
             static_cast<int>(summaryRows.columnInt64(4)), optionalInt(5), optionalInt(6),
             static_cast<int>(summaryRows.columnInt64(7)),
             static_cast<int>(summaryRows.columnInt64(8)),
             static_cast<int>(summaryRows.columnInt64(9)), summaryRows.columnInt64(10) != 0});
    }
    CHECK(summary.size() == storedSummary.size());
    CHECK(summary == storedSummary);
}
