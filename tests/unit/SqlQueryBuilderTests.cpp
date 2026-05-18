#include "query/SqlQueryBuilder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

TEST_CASE("sql query builder uses bound parameters for search text") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "abc' OR 1=1 --";
    request.pageSize = 50;

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("abc") == std::string::npos);
    REQUIRE(queries.page.bindings.size() > 2);
    REQUIRE(queries.page.bindings.back() == "0");
    REQUIRE(queries.page.bindings[queries.page.bindings.size() - 2] == "50");
}

TEST_CASE("sql query builder rejects unknown visible columns") {
    ssa::domain::SsaPageRequest request;
    request.visibleColumns = {"numero_ssa", "not_real"};

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.build(request), std::invalid_argument);
}

TEST_CASE("sql query builder compiles safe pattern search with escaped LIKE") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "~foo.bar";

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("LIKE ? COLLATE NOCASE ESCAPE") != std::string::npos);
    REQUIRE(queries.page.bindings.front() == "FOO_BAR");
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
    REQUIRE(std::ranges::find(queries.page.bindings, "APV") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "SMM") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "202501") != queries.page.bindings.end());
    REQUIRE(std::ranges::find(queries.page.bindings, "202530") != queries.page.bindings.end());
    REQUIRE(queries.page.sql.find("APV") == std::string::npos);
    REQUIRE(queries.page.sql.find("SMM") == std::string::npos);
}

TEST_CASE("sql query builder rejects oversized safe pattern filters") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "~" + std::string(129, 'a');

    REQUIRE_THROWS_AS(ssa::query::SqlQueryBuilder{}.build(request), std::invalid_argument);
}
