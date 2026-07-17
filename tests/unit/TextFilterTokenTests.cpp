#include "domain/TextFilterToken.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("text filter tokens are trimmed prefixed and deduplicated") {
    auto tokens = ssa::domain::parseTextFilterTokens(" =ASE, !ADI,=ASE,  ,");

    REQUIRE(tokens.ordered.size() == 2);
    CHECK(tokens.ordered[0].filterOperator == ssa::domain::TextFilterOperator::Equals);
    CHECK(tokens.ordered[0].value == "ASE");
    CHECK(tokens.ordered[1].filterOperator == ssa::domain::TextFilterOperator::Different);
    CHECK(tokens.ordered[1].value == "ADI");
    CHECK(ssa::domain::joinTextFilterTokens(tokens) == "=ASE,!=ADI");
    CHECK(tokens.indexByValue.contains("ASE"));
    CHECK(tokens.indexByValue.contains("ADI"));
}

TEST_CASE("text filter token builder applies selected operator") {
    ssa::domain::TextFilterTokenSet tokens;

    CHECK(
        ssa::domain::addTextFilterValue(tokens, " APV ", ssa::domain::TextFilterOperator::Equals));
    CHECK(
        ssa::domain::addTextFilterValue(tokens, "ADM", ssa::domain::TextFilterOperator::Different));
    CHECK_FALSE(
        ssa::domain::addTextFilterValue(tokens, "APV", ssa::domain::TextFilterOperator::Equals));

    CHECK(ssa::domain::joinTextFilterTokens(tokens) == "=APV,!=ADM");
}

TEST_CASE("text filter token builder replaces opposite operator for same value") {
    ssa::domain::TextFilterTokenSet tokens;

    CHECK(ssa::domain::addTextFilterValue(tokens, "APV", ssa::domain::TextFilterOperator::Equals));
    CHECK(
        ssa::domain::addTextFilterValue(tokens, "APV", ssa::domain::TextFilterOperator::Different));

    CHECK(ssa::domain::joinTextFilterTokens(tokens) == "!=APV");
    REQUIRE(tokens.ordered.size() == 1);
    CHECK(tokens.ordered[0].filterOperator == ssa::domain::TextFilterOperator::Different);
    CHECK(tokens.indexByValue.contains("APV"));
}

TEST_CASE("text filter ui mode represents equals different and mixed tokens") {
    CHECK(ssa::domain::textFilterUiModeForTokens(ssa::domain::parseTextFilterTokens("!ASE,!ADI")) ==
          ssa::domain::TextFilterUiMode::Different);
    CHECK(ssa::domain::textFilterUiModeForTokens(ssa::domain::parseTextFilterTokens("=ASE,!ADI")) ==
          ssa::domain::TextFilterUiMode::Mixed);
    CHECK(ssa::domain::textFilterUiModeForTokens(ssa::domain::parseTextFilterTokens("")) ==
          ssa::domain::TextFilterUiMode::Equals);
    CHECK(ssa::domain::textFilterUiModeForTokens(ssa::domain::parseTextFilterTokens(""),
                                                 ssa::domain::TextFilterUiMode::Different) ==
          ssa::domain::TextFilterUiMode::Different);
}

TEST_CASE("text filter value list replaces current expression") {
    const auto tokens = ssa::domain::makeTextFilterTokenSet(
        {"ASE", "ADI", "ASE"}, ssa::domain::TextFilterOperator::Different);

    CHECK(ssa::domain::joinTextFilterTokens(tokens) == "!=ASE,!=ADI");
}

TEST_CASE("text filter mutation operator rejects derived mixed mode") {
    CHECK(ssa::domain::textFilterOperatorFromMode("equals").value() ==
          ssa::domain::TextFilterOperator::Equals);
    CHECK(ssa::domain::textFilterOperatorFromMode("different").value() ==
          ssa::domain::TextFilterOperator::Different);
    CHECK_FALSE(ssa::domain::textFilterOperatorFromMode("mixed").has_value());
}

TEST_CASE("text filter token comparison is structural") {
    const auto lhs = ssa::domain::parseTextFilterTokens("=ASE,!ADI");
    const auto rhs = ssa::domain::parseTextFilterTokens(" =ASE, !ADI ");
    const auto differentOrder = ssa::domain::parseTextFilterTokens("!ADI,=ASE");

    CHECK(ssa::domain::sameTextFilterTokens(lhs, rhs));
    CHECK_FALSE(ssa::domain::sameTextFilterTokens(lhs, differentOrder));
}

TEST_CASE("text filter tokens accept legacy and canonical different prefixes") {
    const auto legacy = ssa::domain::parseTextFilterTokens("!ADM");
    const auto canonical = ssa::domain::parseTextFilterTokens("!=ADM");

    REQUIRE(legacy.ordered.size() == 1);
    REQUIRE(canonical.ordered.size() == 1);
    CHECK(legacy.ordered[0].filterOperator == ssa::domain::TextFilterOperator::Different);
    CHECK(canonical.ordered[0].filterOperator == ssa::domain::TextFilterOperator::Different);
    CHECK(legacy.ordered[0].value == "ADM");
    CHECK(canonical.ordered[0].value == "ADM");
    CHECK(ssa::domain::joinTextFilterTokens(legacy) == "!=ADM");
    CHECK(ssa::domain::joinTextFilterTokens(canonical) == "!=ADM");
}
