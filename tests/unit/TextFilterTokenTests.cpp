#include "query/TextFilterToken.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("text filter tokens are trimmed prefixed and deduplicated") {
    auto tokens = ssa::query::parseTextFilterTokens(" =ASE, !ADI,=ASE,  ,");

    REQUIRE(tokens.ordered.size() == 2);
    CHECK(tokens.ordered[0].filterOperator == ssa::query::TextFilterOperator::Equals);
    CHECK(tokens.ordered[0].value == "ASE");
    CHECK(tokens.ordered[1].filterOperator == ssa::query::TextFilterOperator::Different);
    CHECK(tokens.ordered[1].value == "ADI");
    CHECK(ssa::query::joinTextFilterTokens(tokens) == "=ASE,!ADI");
    CHECK(tokens.indexByValue.contains("ASE"));
    CHECK(tokens.indexByValue.contains("ADI"));
}

TEST_CASE("text filter token builder applies selected operator") {
    ssa::query::TextFilterTokenSet tokens;

    CHECK(ssa::query::addTextFilterValue(tokens, " APV ", ssa::query::TextFilterOperator::Equals));
    CHECK(ssa::query::addTextFilterValue(tokens, "ADM", ssa::query::TextFilterOperator::Different));
    CHECK_FALSE(
        ssa::query::addTextFilterValue(tokens, "APV", ssa::query::TextFilterOperator::Equals));

    CHECK(ssa::query::joinTextFilterTokens(tokens) == "=APV,!ADM");
}

TEST_CASE("text filter token builder replaces opposite operator for same value") {
    ssa::query::TextFilterTokenSet tokens;

    CHECK(ssa::query::addTextFilterValue(tokens, "APV", ssa::query::TextFilterOperator::Equals));
    CHECK(ssa::query::addTextFilterValue(tokens, "APV", ssa::query::TextFilterOperator::Different));

    CHECK(ssa::query::joinTextFilterTokens(tokens) == "!APV");
    REQUIRE(tokens.ordered.size() == 1);
    CHECK(tokens.ordered[0].filterOperator == ssa::query::TextFilterOperator::Different);
    CHECK(tokens.indexByValue.contains("APV"));
}

TEST_CASE("text filter ui mode represents equals different and mixed tokens") {
    CHECK(ssa::query::textFilterUiModeForTokens(ssa::query::parseTextFilterTokens("!ASE,!ADI")) ==
          ssa::query::TextFilterUiMode::Different);
    CHECK(ssa::query::textFilterUiModeForTokens(ssa::query::parseTextFilterTokens("=ASE,!ADI")) ==
          ssa::query::TextFilterUiMode::Mixed);
    CHECK(ssa::query::textFilterUiModeForTokens(ssa::query::parseTextFilterTokens("")) ==
          ssa::query::TextFilterUiMode::Equals);
    CHECK(ssa::query::textFilterUiModeForTokens(ssa::query::parseTextFilterTokens(""),
                                                ssa::query::TextFilterUiMode::Different) ==
          ssa::query::TextFilterUiMode::Different);
}

TEST_CASE("text filter value list replaces current expression") {
    const auto tokens = ssa::query::makeTextFilterTokenSet(
        {"ASE", "ADI", "ASE"}, ssa::query::TextFilterOperator::Different);

    CHECK(ssa::query::joinTextFilterTokens(tokens) == "!ASE,!ADI");
}

TEST_CASE("text filter mutation operator rejects derived mixed mode") {
    CHECK(ssa::query::textFilterOperatorFromMode("equals").value() ==
          ssa::query::TextFilterOperator::Equals);
    CHECK(ssa::query::textFilterOperatorFromMode("different").value() ==
          ssa::query::TextFilterOperator::Different);
    CHECK_FALSE(ssa::query::textFilterOperatorFromMode("mixed").has_value());
}

TEST_CASE("text filter token comparison is structural") {
    const auto lhs = ssa::query::parseTextFilterTokens("=ASE,!ADI");
    const auto rhs = ssa::query::parseTextFilterTokens(" =ASE, !ADI ");
    const auto differentOrder = ssa::query::parseTextFilterTokens("!ADI,=ASE");

    CHECK(ssa::query::sameTextFilterTokens(lhs, rhs));
    CHECK_FALSE(ssa::query::sameTextFilterTokens(lhs, differentOrder));
}
