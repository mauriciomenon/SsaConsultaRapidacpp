// Golden values come from the Python reference (armazenamento/derivadas_sync.py:
// _matrix_from_active_sources, _build_closure_rows, _build_summary_rows,
// _analyze_reconciliation, _graph_fingerprint) run on the same synthetic graph.
#include "domain/DerivationGraph.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace {

    namespace derivation = ssa::domain::derivation;

    constexpr auto kA = "202500001";
    constexpr auto kB = "202500002";
    constexpr auto kC = "202500003";
    constexpr auto kD = "202500004";
    constexpr auto kE = "202500005";
    constexpr auto kF = "202500006";
    constexpr auto kX = "202500007";
    constexpr auto kY = "202500008";
    constexpr auto kZ = "202500009";

    derivation::SourceEdge db(const std::string& parent, const std::string& child) {
        return {parent,
                child,
                std::string{derivation::kSourceDbField},
                derivation::kSourceFlagDb,
                derivation::kRelationDbField,
                std::nullopt};
    }

    derivation::SourceEdge sheet(const std::string& parent, const std::string& child,
                                 std::optional<std::string> label) {
        return {parent,
                child,
                std::string{derivation::kSourceSheet},
                derivation::kSourceFlagSheet,
                derivation::kRelationSheet,
                std::move(label)};
    }

    // Diamond A->{B,C}->D->E with D multiparent, F orphan parent of A, cycle X->Y->Z->X.
    std::vector<derivation::SourceEdge> goldenSources() {
        return {db(kA, kB),
                db(kA, kC),
                db(kB, kD),
                sheet(kC, kD, "Complementa \"a\"\\"),
                sheet(kA, kB, "Derivada da"),
                sheet(kD, kE, "Consequ\xc3\xaancia a"),
                db(kX, kY),
                db(kY, kZ),
                db(kZ, kX),
                sheet(kF, kA, std::nullopt)};
    }

} // namespace

TEST_CASE("derivation merge keeps one edge per pair with the highest-priority relation") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);

    REQUIRE(edges.size() == 9);
    CHECK(edges[0] == derivation::MatrixEdge{kA, kB, 3, 2, "Derivada da"});
    CHECK(edges[1] == derivation::MatrixEdge{kA, kC, 1, 1, std::nullopt});
    CHECK(edges[3] == derivation::MatrixEdge{kC, kD, 2, 2, "Complementa \"a\"\\"});
    CHECK(edges[5] == derivation::MatrixEdge{kF, kA, 2, 2, std::nullopt});
}

TEST_CASE("derivation merge keeps the sheet relation when the db source comes after it") {
    const std::vector sources{sheet(kA, kB, "Complementa a"), db(kA, kB)};
    const auto edges = derivation::mergeSources(sources);

    REQUIRE(edges.size() == 1);
    CHECK(edges[0] == derivation::MatrixEdge{kA, kB, 3, 2, "Complementa a"});
}

TEST_CASE("derivation merge drops an empty label like the persisted Python matrix") {
    const std::vector sources{sheet(kA, kB, std::string{})};
    const auto edges = derivation::mergeSources(sources);

    REQUIRE(edges.size() == 1);
    CHECK_FALSE(edges[0].label.has_value());
}

TEST_CASE("derivation closure matches the Python reference, cycles included") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);
    const auto closure = derivation::buildClosure(edges);

    const std::vector<derivation::ClosureRow> expected{
        {kA, kB, 1, 1, 1},  {kA, kC, 1, 1, 1},  {kA, kD, 2, 2, 2},  {kA, kE, 3, 3, 2},
        {kB, kD, 1, 1, 1},  {kB, kE, 2, 2, 1},  {kC, kD, 1, 1, 1},  {kC, kE, 2, 2, 1},
        {kD, kE, 1, 1, 1},  {kF, kA, 1, 1, 1},  {kF, kB, 2, 2, 1},  {kF, kC, 2, 2, 1},
        {kF, kD, 3, 3, 2},  {kF, kE, 4, 4, 2},  {kX, kY, 1, 31, 1}, {kX, kZ, 2, 32, 1},
        {kY, kX, 2, 32, 1}, {kY, kZ, 1, 31, 1}, {kZ, kX, 1, 31, 1}, {kZ, kY, 2, 32, 1},
    };
    CHECK(closure.rows == expected);
    CHECK(closure.cycleNodes == std::set<std::string>{kX, kY, kZ});
}

TEST_CASE("derivation summary matches the Python reference") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);
    const auto summary = derivation::buildSummary(edges, derivation::buildClosure(edges));

    // ssa, parents, children, ancestors, descendants, level min, level max, above, below,
    // component, cycle.
    const std::vector<derivation::SummaryRow> expected{
        {kA, 1, 2, 1, 4, 1, 1, 1, 3, 6, false},
        {kB, 1, 1, 2, 2, 2, 2, 2, 2, 6, false},
        {kC, 1, 1, 2, 2, 2, 2, 2, 2, 6, false},
        {kD, 2, 1, 4, 1, 3, 3, 3, 1, 6, false},
        {kE, 1, 0, 5, 0, 4, 4, 4, 0, 6, false},
        {kF, 0, 1, 0, 5, 0, 0, 0, 4, 6, false},
        {kX, 1, 1, 2, 2, std::nullopt, std::nullopt, 0, 32, 3, true},
        {kY, 1, 1, 2, 2, std::nullopt, std::nullopt, 0, 32, 3, true},
        {kZ, 1, 1, 2, 2, std::nullopt, std::nullopt, 0, 32, 3, true},
    };
    CHECK(summary == expected);
}

TEST_CASE("derivation reconciliation counts multiparent, orphans, conflicts and cycles") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);
    const auto result = derivation::reconcile({kA, kB, kC, kD, kX, kY, kZ}, edges, sources);

    CHECK(result.multiparentChildren == 1);
    CHECK(result.orphanParents == 1);
    CHECK(result.orphanChildren == 1);
    CHECK(result.dbVsSheetConflicts == 1);
    CHECK(result.cycleNodes == 3);
}

TEST_CASE("derivation fingerprint payload is the byte stream the Python v2 hash reads") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);

    // sha256 of this payload is d108f14bcfd7a05bda54d1cab256b28ddfd66311754a83e6c9f222b2882737da,
    // the Python _graph_fingerprint of the same graph.
    CHECK(derivation::fingerprintPayload(edges) ==
          "202500001->202500002:3;[2, \"Derivada da\"];"
          "202500001->202500003:1;[1, null];"
          "202500002->202500004:1;[1, null];"
          "202500003->202500004:2;[2, \"Complementa \\\"a\\\"\\\\\"];"
          "202500004->202500005:2;[2, \"Consequ\xc3\xaancia a\"];"
          "202500006->202500001:2;[2, null];"
          "202500007->202500008:1;[1, null];"
          "202500008->202500009:1;[1, null];"
          "202500009->202500007:1;[1, null];");
}

TEST_CASE("derivation closure stops when cancellation is requested") {
    const auto sources = goldenSources();
    const auto edges = derivation::mergeSources(sources);
    std::stop_source stop;
    stop.request_stop();

    CHECK_THROWS_AS(derivation::buildClosure(edges, derivation::kClosureDepthCap, stop.get_token()),
                    std::runtime_error);
}
