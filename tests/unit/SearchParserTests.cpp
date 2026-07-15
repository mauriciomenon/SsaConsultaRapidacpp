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
    REQUIRE(expression.requiredTerms[3].mode == ssa::domain::MatchMode::SafePattern);
}

TEST_CASE("search parser reserves negation for bang and preserves hyphen") {
    const auto expression = ssa::query::SearchParser{}.parse("-valor,!^adm,!=STE,!$fim,!~padrao");

    REQUIRE(expression.requiredTerms.size() == 5);

    REQUIRE(expression.requiredTerms[0].text == "-valor");
    REQUIRE_FALSE(expression.requiredTerms[0].negated);
    REQUIRE(expression.requiredTerms[0].mode == ssa::domain::MatchMode::Contains);

    REQUIRE(expression.requiredTerms[1].text == "adm");
    REQUIRE(expression.requiredTerms[1].negated);
    REQUIRE(expression.requiredTerms[1].mode == ssa::domain::MatchMode::StartsWith);

    REQUIRE(expression.requiredTerms[2].text == "STE");
    REQUIRE(expression.requiredTerms[2].negated);
    REQUIRE(expression.requiredTerms[2].mode == ssa::domain::MatchMode::Equals);

    REQUIRE(expression.requiredTerms[3].text == "fim");
    REQUIRE(expression.requiredTerms[3].negated);
    REQUIRE(expression.requiredTerms[3].mode == ssa::domain::MatchMode::EndsWith);

    REQUIRE(expression.requiredTerms[4].text == "padrao");
    REQUIRE(expression.requiredTerms[4].negated);
    REQUIRE(expression.requiredTerms[4].mode == ssa::domain::MatchMode::SafePattern);
}
