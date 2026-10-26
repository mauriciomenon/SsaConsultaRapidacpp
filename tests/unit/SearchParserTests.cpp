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

TEST_CASE("search parser negates with leading hyphen like the Python GUI") {
    // Python _parse_search_chunk: a leading '!' or '-' negates when more text follows.
    const auto expression = ssa::query::SearchParser{}.parse("-valor,-=STE,-,!-x");

    REQUIRE(expression.requiredTerms.size() == 4);
    REQUIRE(expression.requiredTerms[0].text == "valor");
    REQUIRE(expression.requiredTerms[0].negated);
    REQUIRE(expression.requiredTerms[0].mode == ssa::domain::MatchMode::Contains);
    REQUIRE(expression.requiredTerms[1].text == "STE");
    REQUIRE(expression.requiredTerms[1].negated);
    REQUIRE(expression.requiredTerms[1].mode == ssa::domain::MatchMode::Equals);
    REQUIRE(expression.requiredTerms[2].text == "-");
    REQUIRE_FALSE(expression.requiredTerms[2].negated);
    REQUIRE(expression.requiredTerms[3].text == "-x");
    REQUIRE(expression.requiredTerms[3].negated);
}

TEST_CASE("search parser negates with bang in every mode") {
    const auto expression =
        ssa::query::SearchParser{}.parse("-valor,!ADM,!^adm,!=STE,!$fim,!~padrao");

    REQUIRE(expression.requiredTerms.size() == 6);

    REQUIRE(expression.requiredTerms[0].text == "valor");
    REQUIRE(expression.requiredTerms[0].negated);
    REQUIRE(expression.requiredTerms[0].mode == ssa::domain::MatchMode::Contains);

    REQUIRE(expression.requiredTerms[1].text == "ADM");
    REQUIRE(expression.requiredTerms[1].negated);
    REQUIRE(expression.requiredTerms[1].mode == ssa::domain::MatchMode::Contains);

    REQUIRE(expression.requiredTerms[2].text == "adm");
    REQUIRE(expression.requiredTerms[2].negated);
    REQUIRE(expression.requiredTerms[2].mode == ssa::domain::MatchMode::StartsWith);

    REQUIRE(expression.requiredTerms[3].text == "STE");
    REQUIRE(expression.requiredTerms[3].negated);
    REQUIRE(expression.requiredTerms[3].mode == ssa::domain::MatchMode::Equals);

    REQUIRE(expression.requiredTerms[4].text == "fim");
    REQUIRE(expression.requiredTerms[4].negated);
    REQUIRE(expression.requiredTerms[4].mode == ssa::domain::MatchMode::EndsWith);

    REQUIRE(expression.requiredTerms[5].text == "padrao");
    REQUIRE(expression.requiredTerms[5].negated);
    REQUIRE(expression.requiredTerms[5].mode == ssa::domain::MatchMode::SafePattern);
}

TEST_CASE("search parser keeps not equals distinct from not contains") {
    const auto expression = ssa::query::SearchParser{}.parse("!ADM,!=ADM");

    REQUIRE(expression.requiredTerms.size() == 2);
    REQUIRE(expression.requiredTerms[0].negated);
    REQUIRE(expression.requiredTerms[0].mode == ssa::domain::MatchMode::Contains);
    REQUIRE(expression.requiredTerms[1].negated);
    REQUIRE(expression.requiredTerms[1].mode == ssa::domain::MatchMode::Equals);
}
