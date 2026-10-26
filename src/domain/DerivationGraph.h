#pragma once

// Derivation graph of SSAs (parent -> child edges), ported from the Python reference
// sync (armazenamento/derivadas_sync.py). Pure computation: merging sources, cycle
// detection, transitive closure, per-SSA summary, reconciliation counts and the
// canonical fingerprint payload. Persistence and hashing live in infra.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain::derivation {

    inline constexpr std::string_view kSourceDbField = "db_field";
    inline constexpr std::string_view kSourceSheet = "sheet_derivadas";
    inline constexpr unsigned kSourceFlagDb = 1U;
    inline constexpr unsigned kSourceFlagSheet = 2U;
    inline constexpr int kRelationUnknown = 0;
    inline constexpr int kRelationDbField = 1;
    inline constexpr int kRelationSheet = 2;
    inline constexpr int kClosureDepthCap = 32;
    inline constexpr std::int64_t kPathCountCap = 1'000'000;

    // Higher priority wins the relation type and label of a pair (db 10, sheet 20).
    [[nodiscard]] int sourcePriority(std::string_view sourceName);

    struct SourceEdge final {
        std::string parent;
        std::string child;
        std::string sourceName;
        unsigned sourceFlag{0U};
        int relationType{kRelationUnknown};
        std::optional<std::string> label;
    };

    struct MatrixEdge final {
        std::string parent;
        std::string child;
        unsigned sourceFlags{0U};
        int relationType{kRelationUnknown};
        std::optional<std::string> label;

        bool operator==(const MatrixEdge&) const = default;
    };

    struct ClosureRow final {
        std::string ancestor;
        std::string descendant;
        int minDistance{0};
        int maxDistance{0};
        std::int64_t pathCount{1};

        bool operator==(const ClosureRow&) const = default;
    };

    struct Closure final {
        std::vector<ClosureRow> rows; // sorted by (ancestor, descendant)
        std::set<std::string> cycleNodes;
        std::size_t truncatedAncestors{0};
    };

    struct SummaryRow final {
        std::string ssa;
        int directParents{0};
        int directChildren{0};
        int ancestors{0};
        int descendants{0};
        std::optional<int> levelFromRootMin;
        std::optional<int> levelFromRootMax;
        int levelsAboveMax{0};
        int levelsBelowMax{0};
        int componentSize{1};
        bool hasCycle{false};

        bool operator==(const SummaryRow&) const = default;
    };

    struct Reconciliation final {
        std::size_t multiparentChildren{0};
        std::size_t orphanParents{0};
        std::size_t orphanChildren{0};
        std::size_t dbVsSheetConflicts{0};
        std::size_t cycleNodes{0};
    };

    // One matrix edge per (parent, child): OR of the source flags; relation type and
    // label from the highest-priority source that has a non-empty value. Sorted by pair.
    [[nodiscard]] std::vector<MatrixEdge> mergeSources(std::span<const SourceEdge> sources);

    // Nodes inside a directed cycle (strongly connected component with 2+ nodes).
    [[nodiscard]] std::set<std::string> cycleNodes(std::span<const MatrixEdge> edges);

    // Per ancestor: shortest distance and number of shortest paths (BFS), and the longest
    // depth found within the cap. Throws std::runtime_error when stop is requested.
    [[nodiscard]] Closure buildClosure(std::span<const MatrixEdge> edges,
                                       int depthCap = kClosureDepthCap,
                                       const std::stop_token& stopToken = {});

    // One row per SSA present in an edge, sorted by SSA number.
    [[nodiscard]] std::vector<SummaryRow> buildSummary(std::span<const MatrixEdge> edges,
                                                       const Closure& closure);

    [[nodiscard]] Reconciliation reconcile(const std::set<std::string>& knownSsas,
                                           std::span<const MatrixEdge> edges,
                                           std::span<const SourceEdge> sources);

    // Bytes hashed by the Python "v2:" fingerprint (SHA-256 is applied by the caller):
    // per edge sorted by pair, `parent->child:flags;[type, label-as-JSON];`.
    [[nodiscard]] std::string fingerprintPayload(std::span<const MatrixEdge> edges);

} // namespace ssa::domain::derivation
