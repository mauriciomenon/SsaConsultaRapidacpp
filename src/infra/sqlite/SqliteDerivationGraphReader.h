#pragma once

// Read-only view of the derivation graph in a SQLite database: the `derivada_de` field of
// ssa_table plus, when the Python sync tables exist, their active sources and matrix.

#include "domain/DerivationGraph.h"

#include <sqlite3.h>

#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace ssa::infra::sqlite {

    struct DerivationGraphSnapshot final {
        std::vector<domain::derivation::SourceEdge> dbFieldEdges;
        std::vector<domain::derivation::SourceEdge> storedSources;
        std::vector<domain::derivation::MatrixEdge> storedMatrix;
        std::set<std::string> knownSsas;
        std::optional<std::string> lastOkFingerprint;
        bool syncTablesPresent{false};
    };

    // Reads everything inside one read transaction, so all parts share a snapshot.
    [[nodiscard]] DerivationGraphSnapshot readDerivationGraph(sqlite3* db);

    // The Python graph fingerprint: SHA-256 of the canonical edge payload, "v2:"-prefixed
    // in the current format.
    [[nodiscard]] std::string derivationGraphFingerprint(
        std::span<const domain::derivation::MatrixEdge> edges,
        domain::derivation::FingerprintFormat format = domain::derivation::FingerprintFormat::V2);

    // Compares in the format of the stored value, as the Python consistency scan does.
    [[nodiscard]] bool
    derivationGraphFingerprintMatches(std::span<const domain::derivation::MatrixEdge> edges,
                                      const std::string& stored);

} // namespace ssa::infra::sqlite
