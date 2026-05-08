#include "query/SqlQueryBuilder.h"

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("sql query builder compiles regex search with REGEXP") {
    ssa::domain::SsaPageRequest request;
    request.searchText = "~foo.*bar";

    const auto queries = ssa::query::SqlQueryBuilder{}.build(request);

    REQUIRE(queries.page.sql.find("REGEXP") != std::string::npos);
}
