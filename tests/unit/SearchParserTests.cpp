#include "query/SearchParser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("search parser splits comma as implicit AND") {
    const auto expression = ssa::query::SearchParser{}.parse("abc, def");

    REQUIRE(expression.requiredTerms.size() == 2);
    REQUIRE(expression.requiredTerms[0].text == "abc");
    REQUIRE(expression.requiredTerms[1].text == "def");
}

TEST_CASE("search parser supports modes and negation") {
    const auto expression = ssa::query::SearchParser{}.parse("!^adm,=STE,foo$,~a.*b");

    REQUIRE(expression.requiredTerms.size() == 4);
    REQUIRE(expression.requiredTerms[0].negated);
    REQUIRE(expression.requiredTerms[0].mode == ssa::domain::MatchMode::StartsWith);
    REQUIRE(expression.requiredTerms[1].mode == ssa::domain::MatchMode::Equals);
    REQUIRE(expression.requiredTerms[2].mode == ssa::domain::MatchMode::EndsWith);
    REQUIRE(expression.requiredTerms[3].mode == ssa::domain::MatchMode::Regex);
}
