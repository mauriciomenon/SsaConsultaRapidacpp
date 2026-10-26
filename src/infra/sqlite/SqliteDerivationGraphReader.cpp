#include "infra/sqlite/SqliteDerivationGraphReader.h"

#include "domain/SsaImportPolicy.h"
#include "infra/sqlite/SqliteConnection.h"

#include <QByteArray>
#include <QCryptographicHash>

#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        namespace derivation = domain::derivation;

        bool tableExists(sqlite3* db, const std::string& name) {
            SqliteStatement statement(
                db, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?");
            statement.bindTextOneBased(1, name);
            return statement.step();
        }

        std::optional<std::string> nullableText(const SqliteStatement& statement,
                                                const int column) {
            if (sqlite3_column_type(statement.handle(), column) == SQLITE_NULL) {
                return std::nullopt;
            }
            return statement.columnText(column);
        }

        // Python collect_db_edges: strict numbers, no self-loops, one edge per pair.
        std::vector<derivation::SourceEdge> readDbFieldEdges(sqlite3* db) {
            SqliteStatement statement(
                db, "SELECT numero_ssa, derivada_de FROM ssa_table WHERE derivada_de IS NOT NULL");
            std::set<std::pair<std::string, std::string>> seen;
            std::vector<derivation::SourceEdge> edges;
            while (statement.step()) {
                auto child = domain::SsaImportPolicy::normalizeNumber(statement.columnText(0));
                auto parent = domain::SsaImportPolicy::normalizeNumber(statement.columnText(1));
                if (child.empty() || parent.empty() || child == parent ||
                    !seen.emplace(parent, child).second) {
                    continue;
                }
                edges.push_back({std::move(parent), std::move(child),
                                 std::string{derivation::kSourceDbField}, derivation::kSourceFlagDb,
                                 derivation::kRelationDbField, std::nullopt});
            }
            return edges;
        }

        std::vector<derivation::SourceEdge> readStoredSources(sqlite3* db) {
            SqliteStatement statement(
                db, "SELECT parent_ssa, child_ssa, source_name, source_flag, relation_type, "
                    "relation_raw_label FROM ssa_derivada_source WHERE is_active = 1");
            std::vector<derivation::SourceEdge> sources;
            while (statement.step()) {
                sources.push_back(
                    {statement.columnText(0), statement.columnText(1), statement.columnText(2),
                     static_cast<unsigned>(statement.columnInt64(3)),
                     static_cast<int>(statement.columnInt64(4)), nullableText(statement, 5)});
            }
            return sources;
        }

        std::vector<derivation::MatrixEdge> readStoredMatrix(sqlite3* db) {
            SqliteStatement statement(
                db, "SELECT parent_ssa, child_ssa, source_flags, relation_type, relation_raw_label "
                    "FROM ssa_derivada_matrix WHERE active = 1 ORDER BY parent_ssa, child_ssa");
            std::vector<derivation::MatrixEdge> edges;
            while (statement.step()) {
                edges.push_back({statement.columnText(0), statement.columnText(1),
                                 static_cast<unsigned>(statement.columnInt64(2)),
                                 static_cast<int>(statement.columnInt64(3)),
                                 nullableText(statement, 4)});
            }
            return edges;
        }

        std::optional<std::string> readLastOkFingerprint(sqlite3* db) {
            SqliteStatement statement(
                db, "SELECT graph_fingerprint FROM ssa_derivada_sync_run WHERE status = 'ok' "
                    "ORDER BY sync_run_id DESC LIMIT 1");
            return statement.step() ? nullableText(statement, 0) : std::nullopt;
        }

        std::set<std::string> readKnownSsas(sqlite3* db) {
            SqliteStatement statement(db, "SELECT numero_ssa FROM ssa_table");
            std::set<std::string> numbers;
            while (statement.step()) {
                if (auto number = domain::SsaImportPolicy::normalizeNumber(statement.columnText(0));
                    !number.empty()) {
                    numbers.insert(std::move(number));
                }
            }
            return numbers;
        }

    } // namespace

    DerivationGraphSnapshot readDerivationGraph(sqlite3* db) {
        SqliteReadTransaction transaction(db);
        DerivationGraphSnapshot snapshot;
        snapshot.dbFieldEdges = readDbFieldEdges(db);
        snapshot.knownSsas = readKnownSsas(db);
        snapshot.syncTablesPresent = tableExists(db, "ssa_derivada_source") &&
                                     tableExists(db, "ssa_derivada_matrix") &&
                                     tableExists(db, "ssa_derivada_sync_run");
        if (snapshot.syncTablesPresent) {
            snapshot.storedSources = readStoredSources(db);
            snapshot.storedMatrix = readStoredMatrix(db);
            snapshot.lastOkFingerprint = readLastOkFingerprint(db);
        }
        transaction.commit();
        return snapshot;
    }

    std::string derivationGraphFingerprint(const std::span<const derivation::MatrixEdge> edges,
                                           const derivation::FingerprintFormat format) {
        const auto payload = derivation::fingerprintPayload(edges, format);
        const auto digest =
            QCryptographicHash::hash(QByteArray::fromStdString(payload), QCryptographicHash::Sha256)
                .toHex()
                .toStdString();
        return format == derivation::FingerprintFormat::V2 ? "v2:" + digest : digest;
    }

    bool derivationGraphFingerprintMatches(const std::span<const derivation::MatrixEdge> edges,
                                           const std::string& stored) {
        const auto format = stored.starts_with("v2:") ? derivation::FingerprintFormat::V2
                                                      : derivation::FingerprintFormat::Legacy;
        return derivationGraphFingerprint(edges, format) == stored;
    }

} // namespace ssa::infra::sqlite
