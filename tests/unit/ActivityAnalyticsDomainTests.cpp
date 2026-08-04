#include "domain/ActivityAnalyticsTypes.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    using ssa::domain::AnalyticsPeriod;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::Breakdown;
    using ssa::domain::IsoWeek;
    using ssa::domain::PersonRole;

    AnalyticsRequest validRequest() {
        AnalyticsRequest request;
        request.period = AnalyticsPeriod{{2026, 1}, {2026, 5}};
        return request;
    }

} // namespace

TEST_CASE("activity analytics validates real ISO weeks") {
    CHECK(ssa::domain::isValidIsoWeek({2020, 53}));
    CHECK_FALSE(ssa::domain::isValidIsoWeek({2021, 53}));
    CHECK_FALSE(ssa::domain::isValidIsoWeek({2026, 0}));
    CHECK_FALSE(ssa::domain::isValidIsoWeek({2026, 54}));
}

TEST_CASE("activity analytics maps an ISO week to the month of its Thursday") {
    CHECK(ssa::domain::isoReferenceMonth({2020, 53}) == "2020-12");
    CHECK(ssa::domain::isoReferenceMonth({2021, 1}) == "2021-01");
    CHECK(ssa::domain::isoReferenceMonth({2025, 1}) == "2025-01");
}

TEST_CASE("activity analytics builds stable time bucket keys") {
    using ssa::domain::TimeGrain;

    CHECK(ssa::domain::analyticsBucketKey({2026, 5}, TimeGrain::WholePeriod).empty());
    CHECK(ssa::domain::analyticsBucketKey({2026, 5}, TimeGrain::IsoWeek) == "202605");
    CHECK(ssa::domain::analyticsBucketKey({2020, 53}, TimeGrain::IsoReferenceMonth) == "2020-12");
}

TEST_CASE("activity analytics formats compact ISO week labels for UI") {
    CHECK(ssa::domain::formatIsoYearWeekDisplay(2026, 1) == "202601");
    CHECK(ssa::domain::formatIsoYearWeekDisplay(2026, 31) == "202631");
    CHECK(ssa::domain::formatAnalyticsBucketLabel("202601") == "202601");
    CHECK(ssa::domain::formatAnalyticsBucketLabel("202631") == "202631");
    CHECK(ssa::domain::formatAnalyticsBucketLabel("2026-07") == "2026-07");
}

TEST_CASE("activity analytics validates inclusive periods") {
    CHECK(ssa::domain::isValidPeriod({{2020, 53}, {2021, 1}}));
    CHECK_FALSE(ssa::domain::isValidPeriod({{2026, 5}, {2026, 4}}));
    CHECK_FALSE(ssa::domain::isValidPeriod({{2021, 53}, {2022, 1}}));
}

TEST_CASE("activity analytics derives exactly thirteen ISO reference months") {
    CHECK(ssa::domain::referenceMonthHistoryPeriod({2026, 10}, 13) ==
          AnalyticsPeriod{{2025, 10}, {2026, 10}});
    CHECK(ssa::domain::referenceMonthHistoryPeriod({2021, 1}, 13) ==
          AnalyticsPeriod{{2020, 1}, {2021, 1}});
    CHECK(ssa::domain::referenceMonthHistoryPeriod({2020, 53}, 13) ==
          AnalyticsPeriod{{2019, 49}, {2020, 53}});
    CHECK_THROWS_AS(ssa::domain::referenceMonthHistoryPeriod({2026, 10}, 0), std::invalid_argument);
}

TEST_CASE("activity analytics maps a calendar month to its complete ISO week range") {
    CHECK(ssa::domain::calendarMonthPeriod(2026, 7) == AnalyticsPeriod{{2026, 27}, {2026, 31}});
    CHECK(ssa::domain::calendarMonthPeriod(2021, 1) == AnalyticsPeriod{{2020, 53}, {2021, 4}});
    CHECK_THROWS_AS(ssa::domain::calendarMonthPeriod(2026, 13), std::invalid_argument);
}

TEST_CASE("activity analytics builds person initials tags") {
    CHECK(ssa::domain::personInitialsTag("Joao Silva Santos") == "JSS");
    CHECK(ssa::domain::personInitialsTag("Maria") == "M");
}

TEST_CASE("activity analytics ignores aggregate total tags case-insensitively") {
    CHECK(ssa::domain::chartSeriesTag("total").empty());
    CHECK(ssa::domain::chartSeriesTag("Total").empty());
    CHECK(ssa::domain::chartSeriesTag("TOTAL").empty());
}

TEST_CASE("activity analytics ISO reference month period differs from calendar month at boundary") {
    const auto calendar = ssa::domain::calendarMonthPeriod(2021, 1);
    const auto isoMonth = ssa::domain::isoReferenceMonthPeriod(2021, 1);
    CHECK(isoMonth != calendar);
    CHECK(isoMonth.first == IsoWeek{2021, 1});
    CHECK(isoMonth.last == IsoWeek{2021, 4});
}

TEST_CASE("activity analytics ISO reference month period terminates at the supported range end") {
    const auto isoMonth = ssa::domain::isoReferenceMonthPeriod(2999, 12);
    CHECK(isoMonth.first.year == 2999);
    CHECK(isoMonth.last.year == 2999);
    CHECK(isoMonth.first <= isoMonth.last);
    CHECK_THROWS_AS(ssa::domain::isoReferenceMonthPeriod(3000, 1), std::invalid_argument);
}

TEST_CASE("activity analytics exposes ISO reference month parts without string parsing") {
    const auto parts = ssa::domain::isoReferenceMonthParts(IsoWeek{2025, 1});
    REQUIRE(parts.has_value());
    CHECK(parts->year == 2025);
    CHECK(parts->month == 1);
    CHECK(ssa::domain::isoReferenceMonth(IsoWeek{2025, 1}) == "2025-01");
    CHECK_FALSE(ssa::domain::isoReferenceMonthParts(IsoWeek{2025, 54}).has_value());
    CHECK_FALSE(ssa::domain::isoReferenceMonthParts(IsoWeek{3000, 1}).has_value());
}

TEST_CASE("activity analytics year-to-date uses ISO reference month at year boundary") {
    const auto selection = ssa::domain::yearToDateCalendarSelection("2024-12-31");
    REQUIRE(selection.has_value());
    CHECK(selection->year == 2025);
    CHECK(selection->month == 1);
    CHECK(selection->first == IsoWeek{2025, 1});
    CHECK(selection->last == IsoWeek{2025, 1});
}

TEST_CASE("activity analytics calculates an ordinary least squares trend") {
    const std::array values{1.0, 3.0, 5.0, 7.0};
    const auto trend = ssa::domain::linearTrend(values);

    REQUIRE(trend.size() == 4);
    CHECK(trend[0] == Catch::Approx(1.0));
    CHECK(trend[1] == Catch::Approx(3.0));
    CHECK(trend[2] == Catch::Approx(5.0));
    CHECK(trend[3] == Catch::Approx(7.0));
    const std::array singleValue{4.0};
    const std::array<double, 0> emptyValues{};
    CHECK(ssa::domain::linearTrend(singleValue).empty());
    CHECK(ssa::domain::linearTrend(emptyValues).empty());
}

TEST_CASE("activity analytics rejects invalid warning windows") {
    auto request = validRequest();
    request.metric = ssa::domain::AnalyticsMetric::PendingDeadline;

    CHECK(ssa::domain::validateAnalyticsRequest(request) ==
          std::optional<std::string>{"warning window is required for deadline analytics"});
    request.warningWindowDays = -1;
    CHECK(ssa::domain::validateAnalyticsRequest(request) ==
          std::optional<std::string>{"warning window must be between 0 and 365 days"});
    request.warningWindowDays = 366;
    CHECK(ssa::domain::validateAnalyticsRequest(request) ==
          std::optional<std::string>{"warning window must be between 0 and 365 days"});
    request.warningWindowDays = 14;
    CHECK_FALSE(ssa::domain::validateAnalyticsRequest(request).has_value());
}

TEST_CASE("activity analytics requires explicit people for person breakdowns") {
    auto request = validRequest();
    request.breakdown = Breakdown::DivisionPerson;
    request.personRole = PersonRole::Executor;

    CHECK(ssa::domain::validateAnalyticsRequest(request) ==
          std::optional<std::string>{"person breakdown requires an explicit selection"});
    request.people = {"Ana"};
    CHECK_FALSE(ssa::domain::validateAnalyticsRequest(request).has_value());

    request.breakdown = Breakdown::DivisionSectorPerson;
    request.people.clear();
    CHECK(ssa::domain::validateAnalyticsRequest(request) ==
          std::optional<std::string>{"person breakdown requires an explicit selection"});
}

TEST_CASE("activity analytics classifies registration cohorts") {
    const AnalyticsPeriod period{{2026, 2}, {2026, 5}};

    CHECK(ssa::domain::registrationCohort(202601, period) ==
          ssa::domain::RegistrationCohort::RegisteredBeforePeriod);
    CHECK(ssa::domain::registrationCohort(202603, period) ==
          ssa::domain::RegistrationCohort::RegisteredInPeriod);
    CHECK(ssa::domain::registrationCohort(std::nullopt, period) ==
          ssa::domain::RegistrationCohort::RegistrationUnknown);
}

TEST_CASE("activity analytics classifies pending deadlines") {
    using ssa::domain::DeadlineClass;

    CHECK(ssa::domain::classifyDeadline("Fora de Prazo", 20, 14) == DeadlineClass::Overdue);
    CHECK(ssa::domain::classifyDeadline("Dentro do Prazo", -1, 14) == DeadlineClass::Overdue);
    CHECK(ssa::domain::classifyDeadline("Dentro do Prazo", 0, 14) == DeadlineClass::Warning);
    CHECK(ssa::domain::classifyDeadline("Dentro do Prazo", 14, 14) == DeadlineClass::Warning);
    CHECK(ssa::domain::classifyDeadline("Dentro do Prazo", 15, 14) == DeadlineClass::OnTime);
    CHECK(ssa::domain::classifyDeadline("", 5, 14) == DeadlineClass::Warning);
    CHECK(ssa::domain::classifyDeadline("", 20, 14) == DeadlineClass::OnTime);
    CHECK(ssa::domain::classifyDeadline("Nao Se Aplica", 5, 14) ==
          DeadlineClass::NotApplicableOrUnknown);
    CHECK(ssa::domain::classifyDeadline("Dentro do Prazo", std::nullopt, 14) ==
          DeadlineClass::NotApplicableOrUnknown);
}

TEST_CASE("activity analytics normalizes deadline source states") {
    using ssa::domain::DeadlineClass;

    CHECK(ssa::domain::classifyDeadline("  fOrA dE pRaZo  ", 20, 14) == DeadlineClass::Overdue);
    CHECK(ssa::domain::classifyDeadline("  dEnTrO dO pRaZo  ", 14, 14) == DeadlineClass::Warning);
}
