#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"
#include "domain/SafePattern.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

TEST_CASE("sql query builder uses bound parameters for search text") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "abc' OR 1=1 --";
    request.pageSize = 50;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    const std::string expectedPageSize = "50";
    const std::string expectedOffset = "0";
    REQUIRE(queries.page.sql.find("abc") == std::string::npos);
    REQUIRE(queries.page.bindings.size() > 2);
    REQUIRE(queries.page.bindings.back() == expectedOffset);
    REQUIRE(queries.page.bindings[queries.page.bindings.size() - 2] == expectedPageSize);
}

TEST_CASE("sql query builder rejects unknown visible columns") {
    ssa::domain::SsaPageRequest request;
    request.visibleColumns = {"numero_ssa", "not_real"};

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.build(request), std::invalid_argument);
}

TEST_CASE("sql query builder selects and orders derived count column as expression") {
    ssa::domain::SsaPageRequest request;
    request.visibleColumns = {"numero_ssa",
                              std::string{ssa::domain::ColumnCatalog::derivedCountColumnKey()}};
    request.sort.columnKey = std::string{ssa::domain::ColumnCatalog::derivedCountColumnKey()};
    request.sort.ascending = false;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"qtd_derivadas\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("COUNT(*)") != std::string::npos);
    REQUIRE(queries.page.sql.find("LEFT JOIN") != std::string::npos);
    REQUIRE(queries.page.sql.find("GROUP BY TRIM(COALESCE(\"derivada_de\", ''))") !=
            std::string::npos);
    const auto orderBy = queries.page.sql.find(" ORDER BY ");
    REQUIRE(orderBy != std::string::npos);
    REQUIRE(queries.page.sql.find("COALESCE(\"derived_counts\".\"qtd_derivadas\", 0)", orderBy) !=
            std::string::npos);
}

TEST_CASE("sql query builder rejects distinct values for derived count column") {
    ssa::domain::DistinctValuesRequest request;
    request.columnKey = std::string{ssa::domain::ColumnCatalog::derivedCountColumnKey()};

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.buildDistinctValues(request),
                      std::invalid_argument);
}

TEST_CASE("sql query builder rejects unsafe table identifiers") {
    ssa::domain::SsaPageRequest request;

    REQUIRE_THROWS_AS(
        ssa::query::SqlQueryBuilder{R"(ssa"; DROP TABLE ssa_table; --)"}.build(request),
        std::invalid_argument);
}

TEST_CASE("sql query builder status-last sort uses a catalog-backed column") {
    REQUIRE(ssa::domain::ColumnCatalog::contains(ssa::domain::ColumnCatalog::statusColumnKey()));

    ssa::domain::SsaPageRequest request;
    request.sort.statusLast = true;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"situacao\"") != std::string::npos);
}

TEST_CASE("sql query builder uses SSA number descending as default order") {
    ssa::domain::SsaPageRequest request;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"numero_ssa\" DESC") != std::string::npos);
}

TEST_CASE("sql query builder orders requested text columns in both directions") {
    ssa::domain::SsaPageRequest request;
    request.sort.columnKey = "situacao";
    request.sort.ascending = true;
    request.sort.statusLast = ssa::domain::shouldApplyStatusLastTieBreaker(request.sort.columnKey);

    auto queries = ssa::query::SqlQueryBuilder{}.build(request);
    REQUIRE(queries.page.sql.find("ORDER BY \"situacao\" ASC") != std::string::npos);

    request.sort.ascending = false;
    queries = ssa::query::SqlQueryBuilder{}.build(request);
    REQUIRE(queries.page.sql.find("ORDER BY \"situacao\" DESC") != std::string::npos);
}

TEST_CASE("sql query builder preserves status-last tie breaker before SSA sort") {
    ssa::domain::SsaPageRequest request;
    request.sort.columnKey = "numero_ssa";
    request.sort.ascending = true;
    request.sort.statusLast = true;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);
    const auto statusCase = queries.page.sql.find("CASE WHEN UPPER(COALESCE(\"situacao\"");
    const auto ssaOrder = queries.page.sql.find("\"numero_ssa\" ASC");

    REQUIRE(statusCase != std::string::npos);
    REQUIRE(ssaOrder != std::string::npos);
    REQUIRE(statusCase < ssaOrder);
}

TEST_CASE("sql query builder compiles safe pattern dot wildcard through LIKE") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "~foo.bar";

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(ssa::domain::kSafePatternWildcard == ".");
    REQUIRE(queries.page.sql.find("LIKE ? COLLATE NOCASE ESCAPE") != std::string::npos);
    REQUIRE(queries.page.bindings.front().starts_with("FOO"));
    REQUIRE(queries.page.bindings.front().at(3) == '_');
    REQUIRE(queries.page.bindings.front().ends_with("BAR"));
}

TEST_CASE("sql query builder compiles advanced week and derivation filters") {
    ssa::domain::SsaPageRequest request;
    request.advancedFilters.weekColumnKey = "semana_programada";
    request.advancedFilters.year = 2025;
    request.advancedFilters.week = 2;
    request.advancedFilters.derivationMode = ssa::domain::DerivationFilterMode::DerivedOnly;
    request.advancedFilters.onlyReprogrammed = true;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"semana_programada\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"derivada_de\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"num_reprogramacoes\"") != std::string::npos);
    REQUIRE(queries.page.bindings[queries.page.bindings.size() - 3] == "202502");
}

TEST_CASE("sql query builder compiles advanced text and range filters") {
    ssa::domain::SsaPageRequest request;
    request.advancedFilters.textFilters = {{"situacao", "=APV"}, {"setor_executor", "=SMM"}};
    request.advancedFilters.issueYear = 2025;
    request.advancedFilters.executionYear = 2025;
    request.advancedFilters.reprogrammingEquals = 1;
    request.advancedFilters.reprogrammingComparison =
        ssa::domain::NumericComparisonMode::GreaterOrEqual;
    request.advancedFilters.issueWeekStart = 202501;
    request.advancedFilters.issueWeekEnd = 202520;
    request.advancedFilters.executionWeekStart = 202503;
    request.advancedFilters.executionWeekEnd = 202530;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"situacao\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"setor_executor\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"semana_cadastro\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"semana_executada\"") != std::string::npos);
    REQUIRE(queries.page.sql.find("\"num_reprogramacoes\"") != std::string::npos);
    REQUIRE(queries.page.sql.find(">= ?") != std::string::npos);
    REQUIRE(std::ranges::find(queries.page.bindings, "APV") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "SMM") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "202501") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "202530") != queries.page.bindings.end());
    REQUIRE(queries.page.sql.find("APV") == std::string::npos);
    REQUIRE(queries.page.sql.find("SMM") == std::string::npos);
}

TEST_CASE("sql query builder compiles reprogramming multi-select as bound values") {
    ssa::domain::SsaPageRequest request;
    request.advancedFilters.reprogrammingValues = {1, 3, 5};
    request.advancedFilters.reprogrammingComparison = ssa::domain::NumericComparisonMode::Equals;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("\"num_reprogramacoes\"") != std::string::npos);
    REQUIRE(queries.page.sql.find(" IN (?, ?, ?)") != std::string::npos);
    REQUIRE(std::ranges::find(queries.page.bindings, "1") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "3") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "5") != queries.page.bindings.end());
}

TEST_CASE("sql query builder combines basic and advanced filters for the same column") {
    ssa::domain::SsaPageRequest request;
    request.columnFilters = {{"situacao", "=APL"}};
    request.advancedFilters.textFilters = {{"situacao", "!SPG"}};

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(std::ranges::find(queries.page.bindings, "APL") != queries.page.bindings.end());
    REQUIRE(std::ranges::any_of(queries.page.bindings, [](const std::string& binding) {
        return binding.find("SPG") != std::string::npos;
    }));
}

TEST_CASE("sql query builder binds distinct values limit") {
    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "situacao";
    request.limit = 37;

    const auto query = ssa::query::SqlQueryBuilder{}.buildDistinctValues(request);

    REQUIRE(query.sql.find("LIMIT ?") != std::string::npos);
    REQUIRE_FALSE(query.bindings.empty());
    REQUIRE(query.bindings.back() == "37");
}

TEST_CASE("sql query builder can order distinct values by filtered frequency") {
    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "responsavel_execucao";
    request.limit = 25;
    request.orderByFrequency = true;

    const auto query = ssa::query::SqlQueryBuilder{}.buildDistinctValues(request);

    REQUIRE(query.sql.find("SELECT DISTINCT") == std::string::npos);
    // Projection/grouping use the trimmed expression so whitespace variants collapse.
    REQUIRE(query.sql.find("GROUP BY TRIM(COALESCE(\"responsavel_execucao\", ''))") !=
            std::string::npos);
    REQUIRE(query.sql.find("ORDER BY COUNT(*) DESC") != std::string::npos);
    REQUIRE(query.bindings.back() == "25");
}

TEST_CASE("sql query builder rejects overflowing page offsets") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 500;
    request.pageIndex = std::numeric_limits<std::size_t>::max();

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.build(request), std::overflow_error);
}

TEST_CASE("sql query builder omits pagination when page size is zero") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 0;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find(" LIMIT ? OFFSET ?") == std::string::npos);
    REQUIRE(std::ranges::find(queries.page.bindings, "0") == queries.page.bindings.end());
}

TEST_CASE("sql query builder rejects oversized safe pattern filters") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "~" + std::string(129, 'a');

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.build(request), std::invalid_argument);
}
