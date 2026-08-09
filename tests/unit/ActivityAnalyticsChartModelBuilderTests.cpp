#include "application/ActivityAnalyticsChartModelBuilder.h"
#include "application/ActivityAnalyticsService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

    using ssa::application::ActivityAnalyticsChartModelBuilder;
    using ssa::application::AnalyticsChartMode;
    using ssa::application::AnalyticsChartScale;
    using ssa::domain::AnalyticsMetric;
    using ssa::domain::AnalyticsPoint;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::AnalyticsSeriesResult;
    using ssa::domain::Breakdown;
    using ssa::domain::DeadlineClass;
    using ssa::domain::PersonRole;
    using ssa::domain::RegistrationCohort;
    using ssa::domain::TimeGrain;

    AnalyticsRequest request(const AnalyticsMetric metric, const TimeGrain grain,
                             const Breakdown breakdown = Breakdown::Division) {
        AnalyticsRequest value{.metric = metric,
                               .period = {{2026, 1}, {2026, 13}},
                               .grain = grain,
                               .breakdown = breakdown,
                               .personRole = PersonRole::Executor};
        if (breakdown == Breakdown::DivisionPerson ||
            breakdown == Breakdown::DivisionSectorPerson) {
            value.people = {"Ana"};
        }
        if (metric == AnalyticsMetric::PendingDeadline) {
            value.warningWindowDays = 7;
        }
        return value;
    }

    AnalyticsPoint point(const std::string_view bucket, const std::string_view division,
                         const std::string_view sector, const std::string_view person,
                         const std::int64_t count) {
        return {.bucketKey = std::string{bucket},
                .division = std::string{division},
                .sector = std::string{sector},
                .person = std::string{person},
                .count = count};
    }

    std::vector<std::optional<double>> valuesOf(const ssa::application::AnalyticsChartModel& model,
                                                const std::string& seriesName) {
        for (const auto& series : model.series) {
            if (series.name == seriesName) {
                return series.values;
            }
        }
        FAIL("series was not found: " << seriesName);
        return {};
    }

} // namespace

TEST_CASE("analytics chart modes cover the fourteen dashboard graph contracts") {
    struct DashboardSpec final {
        AnalyticsMetric metric;
        TimeGrain grain;
        AnalyticsChartMode mode;
        AnalyticsChartScale scale;
    };
    constexpr std::array specs{
        DashboardSpec{AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                      AnalyticsChartMode::SimpleBar, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Registered, TimeGrain::IsoReferenceMonth,
                      AnalyticsChartMode::LineTotal, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Executed, TimeGrain::WholePeriod,
                      AnalyticsChartMode::CohortStacked, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Executed, TimeGrain::IsoReferenceMonth,
                      AnalyticsChartMode::LineTotal, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::PartialAttention, TimeGrain::WholePeriod,
                      AnalyticsChartMode::CohortStacked, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Spg, TimeGrain::WholePeriod,
                      AnalyticsChartMode::CohortStacked, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Apg, TimeGrain::WholePeriod,
                      AnalyticsChartMode::CohortStacked, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Apl, TimeGrain::WholePeriod,
                      AnalyticsChartMode::CohortStacked, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Pending, TimeGrain::WholePeriod,
                      AnalyticsChartMode::SimpleBar, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Pending, TimeGrain::IsoReferenceMonth,
                      AnalyticsChartMode::LineTotal, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Issued, TimeGrain::WholePeriod,
                      AnalyticsChartMode::SimpleBar, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::Issued, TimeGrain::IsoReferenceMonth,
                      AnalyticsChartMode::LineTotal, AnalyticsChartScale::Quantity},
        DashboardSpec{AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek,
                      AnalyticsChartMode::DeadlineStacked, AnalyticsChartScale::Percentage},
        DashboardSpec{AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek,
                      AnalyticsChartMode::DeadlineStacked, AnalyticsChartScale::Quantity},
    };

    for (const auto& spec : specs) {
        auto analyticsRequest = request(spec.metric, spec.grain);
        AnalyticsPoint analyticsPoint =
            point(spec.grain == TimeGrain::WholePeriod ? "" : "202601", "SMI", "SMIN", "Ana", 2);
        if (spec.grain == TimeGrain::IsoReferenceMonth) {
            analyticsPoint.bucketKey = "2026-01";
        }
        if (spec.metric == AnalyticsMetric::PendingDeadline) {
            analyticsPoint.deadlineClass = DeadlineClass::OnTime;
        }
        const auto model = ActivityAnalyticsChartModelBuilder::build(
            analyticsRequest, {.points = {analyticsPoint}}, spec.mode, spec.scale);

        CHECK(model.percentage == (spec.scale == AnalyticsChartScale::Percentage));
        CHECK_FALSE(model.series.empty());
        CHECK(model.total == std::optional<double>{2.0});
    }
}

TEST_CASE("simple bars sort dimension categories and reconcile duplicate values") {
    auto analyticsRequest = request(AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                                    Breakdown::DivisionSectorPerson);
    AnalyticsSeriesResult result{.points = {point("", "SMI", "B", "", 3),
                                            point("", "SMI", "A", "Ana", 4),
                                            point("", "SMI", "A", "Ana", 2)}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::SimpleBar);

    CHECK(model.categories == std::vector<std::string>{"SMI / A / Ana", "SMI / B / Nao atribuido"});
    REQUIRE(model.series.size() == 1);
    CHECK(model.series.front().name == "total");
    CHECK(model.series.front().values == std::vector<std::optional<double>>{6.0, 3.0});
    CHECK(model.series.front().total == std::optional<double>{9.0});
    CHECK(model.total == model.series.front().total);
}

TEST_CASE("cohort stacks use stable keys and reconcile every component") {
    auto analyticsRequest =
        request(AnalyticsMetric::Executed, TimeGrain::WholePeriod, Breakdown::DivisionSector);
    auto inPeriod = point("", "SMI", "SMIN", "Ana", 3);
    inPeriod.cohort = RegistrationCohort::RegisteredInPeriod;
    auto before = point("", "SMI", "SMIN", "Ana", 5);
    before.cohort = RegistrationCohort::RegisteredBeforePeriod;
    auto unknown = point("", "SMI", "SMIN", "Ana", 2);
    unknown.cohort = RegistrationCohort::RegistrationUnknown;

    const auto model = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, {.points = {before, unknown, inPeriod}},
        AnalyticsChartMode::CohortStacked);

    REQUIRE(model.series.size() == 3);
    CHECK(model.series[0].name == "registered_in_period");
    CHECK(model.series[1].name == "registered_before_period");
    CHECK(model.series[2].name == "registration_unknown");
    CHECK(model.series[0].values == std::vector<std::optional<double>>{3.0});
    CHECK(model.series[1].values == std::vector<std::optional<double>>{5.0});
    CHECK(model.series[2].values == std::vector<std::optional<double>>{2.0});
    CHECK(model.series[0].total == std::optional<double>{3.0});
    CHECK(model.series[1].total == std::optional<double>{5.0});
    CHECK(model.series[2].total == std::optional<double>{2.0});
    CHECK(model.total == std::optional<double>{10.0});
}

TEST_CASE("deadline percentage is normalized in the worker and quantity remains raw") {
    auto analyticsRequest = request(AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek);
    auto onTime = point("202601", "SMI", "SMIN", "Ana", 4);
    onTime.deadlineClass = DeadlineClass::OnTime;
    auto warning = point("202601", "SMI", "SMIN", "Ana", 1);
    warning.deadlineClass = DeadlineClass::Warning;
    auto overdue = point("202601", "SMI", "SMIN", "Ana", 2);
    overdue.deadlineClass = DeadlineClass::Overdue;
    auto excluded = point("202601", "SMI", "SMIN", "Ana", 3);
    excluded.deadlineClass = DeadlineClass::NotApplicableOrUnknown;
    const AnalyticsSeriesResult result{.points = {excluded, overdue, onTime, warning},
                                       .excludedForDataQuality = 3};

    const auto quantity = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, result, AnalyticsChartMode::DeadlineStacked,
        AnalyticsChartScale::Quantity);
    const auto percentage = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, result, AnalyticsChartMode::DeadlineStacked,
        AnalyticsChartScale::Percentage);

    CHECK(quantity.categories == std::vector<std::string>{"202601", "202602", "202603", "202604",
                                                          "202605", "202606", "202607", "202608",
                                                          "202609", "202610", "202611", "202612",
                                                          "202613"});
    CHECK(valuesOf(quantity, "on_time").front() == std::optional<double>{4.0});
    CHECK(valuesOf(quantity, "warning").front() == std::optional<double>{1.0});
    CHECK(valuesOf(quantity, "overdue").front() == std::optional<double>{2.0});
    CHECK(valuesOf(percentage, "on_time").front() == Catch::Approx(400.0 / 7.0));
    CHECK(valuesOf(percentage, "warning").front() == Catch::Approx(100.0 / 7.0));
    CHECK(valuesOf(percentage, "overdue").front() == Catch::Approx(200.0 / 7.0));
    CHECK(valuesOf(percentage, "on_time").front().value() +
              valuesOf(percentage, "warning").front().value() +
              valuesOf(percentage, "overdue").front().value() ==
          Catch::Approx(100.0));
    CHECK_FALSE(quantity.percentage);
    CHECK(percentage.percentage);
    CHECK(quantity.total == std::optional<double>{7.0});
    CHECK(percentage.total == quantity.total);
    CHECK(quantity.qualityNote == "excluded_for_data_quality=3");
}

TEST_CASE("deadline quality counter must reconcile with excluded points") {
    auto analyticsRequest = request(AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek);
    auto excluded = point("202601", "SMI", "SMIN", "Ana", 2);
    excluded.deadlineClass = DeadlineClass::NotApplicableOrUnknown;

    CHECK_THROWS_AS(ActivityAnalyticsChartModelBuilder::build(
                        analyticsRequest, {.points = {excluded}, .excludedForDataQuality = 1},
                        AnalyticsChartMode::DeadlineStacked),
                    std::logic_error);
}

TEST_CASE("monthly stock line keeps a real gap and fits OLS on observed positions") {
    auto analyticsRequest = request(AnalyticsMetric::Pending, TimeGrain::IsoReferenceMonth);
    const AnalyticsSeriesResult result{
        .points = {point("2026-03", "SMI", "SMIN", "Ana", 30),
                   point("2026-01", "SMI", "SMIN", "Ana", 10)},
        .observedIsoYearWeek = 202613,
        .observations = {{.bucketKey = "2026-01", .observedIsoYearWeek = 202605},
                         {.bucketKey = "2026-03", .observedIsoYearWeek = 202613}}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::LineTotal);

    CHECK(model.categories == std::vector<std::string>{"2026-01", "2026-02", "2026-03"});
    REQUIRE(model.series.size() == 1);
    CHECK(model.series.front().values ==
          std::vector<std::optional<double>>{10.0, std::nullopt, 30.0});
    REQUIRE(model.series.front().trendValues.size() == 3);
    CHECK(model.series.front().trendValues[0] == Catch::Approx(10.0));
    CHECK(model.series.front().trendValues[1] == Catch::Approx(20.0));
    CHECK(model.series.front().trendValues[2] == Catch::Approx(30.0));
    CHECK(model.subtitle == "Semana observada: 202613");
}

TEST_CASE("event lines use zero for a bucket without a row") {
    auto analyticsRequest = request(AnalyticsMetric::Executed, TimeGrain::IsoReferenceMonth);
    const AnalyticsSeriesResult result{.points = {point("2026-01", "SMI", "SMIN", "Ana", 10),
                                                  point("2026-03", "SMI", "SMIN", "Ana", 30)}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::LineTotal);

    CHECK(model.series.front().values == std::vector<std::optional<double>>{10.0, 0.0, 30.0});
}

TEST_CASE("custom whole-period labels follow all four breakdown contracts") {
    const auto analyticsPoint = point("", "SMI", "SMIN", "Ana", 2);
    const std::array breakdowns{Breakdown::Division, Breakdown::DivisionSector,
                                Breakdown::DivisionPerson, Breakdown::DivisionSectorPerson};
    const std::array expected{"SMI", "SMI / SMIN", "SMI / Ana", "SMI\nSMIN"};

    for (std::size_t index = 0; index < breakdowns.size(); ++index) {
        const auto model = ActivityAnalyticsChartModelBuilder::build(
            request(AnalyticsMetric::Registered, TimeGrain::WholePeriod, breakdowns[index]),
            {.points = {analyticsPoint}}, AnalyticsChartMode::Custom);
        CHECK(model.categories == std::vector<std::string>{expected[index]});
    }
}

TEST_CASE("custom whole-period sector-person breakdown groups people as series") {
    auto analyticsRequest = request(AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                                    Breakdown::DivisionSectorPerson);
    analyticsRequest.sectors = {"IEE2", "IEE3"};
    analyticsRequest.people = {"Ana", "Bia"};
    const AnalyticsSeriesResult result{
        .points = {point("", "IEE", "IEE2", "Ana", 2), point("", "IEE", "IEE2", "Ana", 1),
                   point("", "IEE", "IEE3", "Ana", 4), point("", "IEE", "IEE3", "Bia", 5)}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::Custom);

    CHECK(model.categories == std::vector<std::string>{"IEE\nIEE2", "IEE\nIEE3"});
    REQUIRE(model.series.size() == 2);
    CHECK(model.series[0].name == "Ana");
    CHECK(model.series[0].values == std::vector<std::optional<double>>{3.0, 4.0});
    CHECK(model.series[0].total == std::optional<double>{7.0});
    CHECK(model.series[1].name == "Bia");
    CHECK(model.series[1].values == std::vector<std::optional<double>>{0.0, 5.0});
    CHECK(model.series[1].total == std::optional<double>{5.0});
    CHECK(model.total == std::optional<double>{12.0});
}

TEST_CASE("person role selection does not change dimension labels") {
    const auto analyticsPoint = point("", "SMI", "SMIN", "Ana", 2);
    std::optional<std::vector<std::string>> baseline;

    for (const auto role : {PersonRole::Requester, PersonRole::Planner, PersonRole::Executor}) {
        auto analyticsRequest = request(AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                                        Breakdown::DivisionSectorPerson);
        analyticsRequest.personRole = role;
        const auto model = ActivityAnalyticsChartModelBuilder::build(
            analyticsRequest, {.points = {analyticsPoint}}, AnalyticsChartMode::Custom);
        if (!baseline.has_value()) {
            baseline = model.categories;
        }
        CHECK(model.categories == *baseline);
    }
}

TEST_CASE("custom temporal charts sort combinations and distinguish event zeros from stock gaps") {
    auto eventRequest =
        request(AnalyticsMetric::Registered, TimeGrain::IsoWeek, Breakdown::DivisionSector);
    eventRequest.period = {{2020, 53}, {2021, 2}};
    const AnalyticsSeriesResult result{
        .points = {point("202053", "SMI", "B", "Ana", 2), point("202101", "SMI", "A", "Ana", 3)},
        .observations = {{.bucketKey = "202053", .observedIsoYearWeek = 202053},
                         {.bucketKey = "202101", .observedIsoYearWeek = 202101}}};

    const auto events =
        ActivityAnalyticsChartModelBuilder::build(eventRequest, result, AnalyticsChartMode::Custom);
    auto stockRequest = eventRequest;
    stockRequest.metric = AnalyticsMetric::Pending;
    const auto stocks =
        ActivityAnalyticsChartModelBuilder::build(stockRequest, result, AnalyticsChartMode::Custom);

    CHECK(events.categories == std::vector<std::string>{"202053", "202101", "202102"});
    REQUIRE(events.series.size() == 2);
    CHECK(events.series[0].name == "SMI / A");
    CHECK(events.series[0].values == std::vector<std::optional<double>>{0.0, 3.0, 0.0});
    CHECK(events.series[1].name == "SMI / B");
    CHECK(events.series[1].values == std::vector<std::optional<double>>{2.0, 0.0, 0.0});
    CHECK(stocks.series[0].values == std::vector<std::optional<double>>{0.0, 3.0, std::nullopt});
    CHECK(stocks.series[1].values == std::vector<std::optional<double>>{2.0, 0.0, std::nullopt});
}

TEST_CASE("stock observation metadata distinguishes an observed zero from a missing month") {
    auto analyticsRequest = request(AnalyticsMetric::Pending, TimeGrain::IsoReferenceMonth);
    const AnalyticsSeriesResult result{
        .points = {point("2026-01", "SMI", "SMIN", "Ana", 5)},
        .observedIsoYearWeek = 202613,
        .observations = {{.bucketKey = "2026-01", .observedIsoYearWeek = 202605},
                         {.bucketKey = "2026-03", .observedIsoYearWeek = 202613}}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::LineTotal);

    CHECK(model.series.front().values ==
          std::vector<std::optional<double>>{5.0, std::nullopt, 0.0});
}

TEST_CASE("an observed zero APL snapshot is a zero total rather than a gap") {
    const AnalyticsSeriesResult result{
        .sourceRevision = "revision-1",
        .observedIsoYearWeek = 202613,
        .observations = {{.bucketKey = "", .observedIsoYearWeek = 202613}}};
    const auto analyticsRequest = request(AnalyticsMetric::Apl, TimeGrain::WholePeriod);
    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::SimpleBar);
    const auto cohort = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, result, AnalyticsChartMode::CohortStacked);

    CHECK(model.categories == std::vector<std::string>{"Total"});
    REQUIRE(model.series.size() == 1);
    CHECK(model.series.front().values == std::vector<std::optional<double>>{0.0});
    CHECK(model.series.front().total == std::optional<double>{0.0});
    CHECK(model.total == std::optional<double>{0.0});
    CHECK(cohort.categories == std::vector<std::string>{"Total"});
    REQUIRE(cohort.series.size() == 3);
    for (const auto& series : cohort.series) {
        CHECK(series.values == std::vector<std::optional<double>>{0.0});
        CHECK(series.total == std::optional<double>{0.0});
    }
}

TEST_CASE("line trend is absent until two values are observed") {
    const auto model = ActivityAnalyticsChartModelBuilder::build(
        request(AnalyticsMetric::Pending, TimeGrain::IsoReferenceMonth),
        {.points = {point("2026-01", "SMI", "SMIN", "Ana", 5)}}, AnalyticsChartMode::LineTotal);

    CHECK(model.series.front().trendValues.empty());
}

TEST_CASE("chart aggregation rejects negative counts and checked-sum overflow") {
    auto analyticsRequest = request(AnalyticsMetric::Registered, TimeGrain::WholePeriod);
    const auto maximum = std::numeric_limits<std::int64_t>::max();

    CHECK_THROWS_AS(ActivityAnalyticsChartModelBuilder::build(
                        analyticsRequest, {.points = {point("", "SMI", "", "", -1)}},
                        AnalyticsChartMode::SimpleBar),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        ActivityAnalyticsChartModelBuilder::build(
            analyticsRequest,
            {.points = {point("", "SMI", "", "", maximum), point("", "SMI", "", "", 1)}},
            AnalyticsChartMode::SimpleBar),
        std::overflow_error);
}

TEST_CASE("dashboard builder owns the fourteen request and mode mappings") {
    using ssa::application::ActivityAnalyticsDashboard;

    const ssa::domain::AnalyticsPeriod reportPeriod{{2026, 1}, {2026, 4}};
    const ssa::domain::AnalyticsPeriod historyPeriod{{2025, 49}, {2026, 4}};
    const auto wholeResult = [](const std::int64_t count) {
        return AnalyticsSeriesResult{
            .points = {point("", "DIV", "SEC", "Ana", count)},
            .observedIsoYearWeek = 202604,
            .observations = {{.bucketKey = "", .observedIsoYearWeek = 202604}}};
    };
    const auto monthlyResult = [](const std::int64_t count) {
        return AnalyticsSeriesResult{
            .points = {point("2026-01", "DIV", "SEC", "Ana", count)},
            .observedIsoYearWeek = 202604,
            .observations = {{.bucketKey = "2026-01", .observedIsoYearWeek = 202604}}};
    };

    ActivityAnalyticsDashboard dashboard{
        .registeredBySector = wholeResult(1),
        .registeredMonthly = monthlyResult(2),
        .executedBySector = wholeResult(3),
        .executedMonthly = monthlyResult(4),
        .partialAttentionBySector = wholeResult(5),
        .spgBySector = wholeResult(6),
        .apgBySector = wholeResult(7),
        .aplBySector = wholeResult(8),
        .pendingBySector = wholeResult(9),
        .pendingMonthly = monthlyResult(10),
        .issuedByDivision = wholeResult(11),
        .issuedMonthly = monthlyResult(12),
        .pendingDeadlineWeekly = {.points = {[] {
                                      auto value = point("202601", "DIV", "SEC", "Ana", 13);
                                      value.deadlineClass = DeadlineClass::OnTime;
                                      return value;
                                  }()},
                                  .observedIsoYearWeek = 202604,
                                  .observations = {{.bucketKey = "202601",
                                                    .observedIsoYearWeek = 202601}}},
    };

    const auto charts =
        ActivityAnalyticsChartModelBuilder::buildDashboard(reportPeriod, historyPeriod, dashboard);

    CHECK(charts.registeredBySector.total == std::optional<double>{1.0});
    CHECK(charts.registeredMonthly.total == std::optional<double>{2.0});
    CHECK(charts.executedBySector.total == std::optional<double>{3.0});
    CHECK(charts.executedMonthly.total == std::optional<double>{4.0});
    CHECK(charts.partialAttentionBySector.total == std::optional<double>{5.0});
    CHECK(charts.spgBySector.total == std::optional<double>{6.0});
    CHECK(charts.apgBySector.total == std::optional<double>{7.0});
    CHECK(charts.aplBySector.total == std::optional<double>{8.0});
    CHECK(charts.pendingBySector.total == std::optional<double>{9.0});
    CHECK(charts.pendingMonthly.total == std::optional<double>{10.0});
    CHECK(charts.issuedByDivision.total == std::optional<double>{11.0});
    CHECK(charts.issuedMonthly.total == std::optional<double>{12.0});
    CHECK(charts.pendingDeadlinePercentage.total == std::optional<double>{13.0});
    CHECK(charts.pendingDeadlineQuantity.total == std::optional<double>{13.0});

    CHECK(charts.registeredBySector.categories == std::vector<std::string>{"DIV / SEC"});
    CHECK(charts.issuedByDivision.categories == std::vector<std::string>{"DIV"});
    CHECK(charts.registeredMonthly.categories == std::vector<std::string>{"2025-12", "2026-01"});
    CHECK(charts.pendingDeadlineQuantity.categories ==
          std::vector<std::string>{"202601", "202602", "202603", "202604"});
    CHECK(charts.executedBySector.series.size() == 3);
    CHECK(charts.partialAttentionBySector.series.size() == 3);
    CHECK(charts.spgBySector.series.size() == 3);
    CHECK(charts.apgBySector.series.size() == 3);
    CHECK(charts.aplBySector.series.size() == 3);
    CHECK(charts.pendingBySector.series.size() == 1);
    CHECK_FALSE(charts.pendingDeadlineQuantity.percentage);
    CHECK(charts.pendingDeadlinePercentage.percentage);
    CHECK(valuesOf(charts.pendingDeadlineQuantity, "on_time").front() ==
          std::optional<double>{13.0});
    CHECK(valuesOf(charts.pendingDeadlinePercentage, "on_time").front() ==
          std::optional<double>{100.0});
}

TEST_CASE("custom monthly series calculate trend without replacing stock gaps") {
    auto analyticsRequest =
        request(AnalyticsMetric::Pending, TimeGrain::IsoReferenceMonth, Breakdown::Division);
    const AnalyticsSeriesResult result{
        .points = {point("2026-01", "SMI", "", "", 10), point("2026-03", "SMI", "", "", 30)},
        .observations = {{.bucketKey = "2026-01", .observedIsoYearWeek = 202605},
                         {.bucketKey = "2026-03", .observedIsoYearWeek = 202613}}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::Custom);

    REQUIRE(model.series.size() == 1);
    CHECK(model.series.front().values ==
          std::vector<std::optional<double>>{10.0, std::nullopt, 30.0});
    REQUIRE(model.series.front().trendValues.size() == 3);
    CHECK(model.series.front().trendValues[0] == Catch::Approx(10.0));
    CHECK(model.series.front().trendValues[1] == Catch::Approx(20.0));
    CHECK(model.series.front().trendValues[2] == Catch::Approx(30.0));
}

TEST_CASE("custom charts preserve explicitly selected zero-value combinations") {
    auto eventRequest =
        request(AnalyticsMetric::Registered, TimeGrain::IsoWeek, Breakdown::DivisionPerson);
    eventRequest.period = {{2026, 1}, {2026, 2}};
    eventRequest.divisions = {"SMI"};
    eventRequest.people = {"Ana", "Bia"};
    const AnalyticsSeriesResult result{
        .points = {point("202601", "SMI", "", "Ana", 3)},
        .observations = {{.bucketKey = "202601", .observedIsoYearWeek = 202601}}};

    const auto events =
        ActivityAnalyticsChartModelBuilder::build(eventRequest, result, AnalyticsChartMode::Custom);
    auto stockRequest = eventRequest;
    stockRequest.metric = AnalyticsMetric::Pending;
    const auto stocks =
        ActivityAnalyticsChartModelBuilder::build(stockRequest, result, AnalyticsChartMode::Custom);
    auto wholeRequest = eventRequest;
    wholeRequest.grain = TimeGrain::WholePeriod;
    const auto whole = ActivityAnalyticsChartModelBuilder::build(
        wholeRequest, AnalyticsSeriesResult{}, AnalyticsChartMode::Custom);

    REQUIRE(events.series.size() == 2);
    CHECK(events.series[0].name == "SMI / Ana");
    CHECK(events.series[1].name == "SMI / Bia");
    CHECK(events.series[1].values == std::vector<std::optional<double>>{0.0, 0.0});
    CHECK(stocks.series[1].values == std::vector<std::optional<double>>{0.0, std::nullopt});
    CHECK(whole.categories == std::vector<std::string>{"SMI / Ana", "SMI / Bia"});
    REQUIRE(whole.series.size() == 1);
    CHECK(whole.series.front().values == std::vector<std::optional<double>>{0.0, 0.0});
}

TEST_CASE("custom charts populate series tags for person breakdowns") {
    auto analyticsRequest =
        request(AnalyticsMetric::Executed, TimeGrain::WholePeriod, Breakdown::DivisionSectorPerson);
    analyticsRequest.divisions = {"SMI"};
    analyticsRequest.sectors = {"SMIN"};
    analyticsRequest.people = {"Joao Silva Santos"};
    const AnalyticsSeriesResult result{
        .points = {point("", "SMI", "SMIN", "Joao Silva Santos", 2)},
        .observations = {{.bucketKey = "", .observedIsoYearWeek = 202613}}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(analyticsRequest, result,
                                                                 AnalyticsChartMode::Custom);

    REQUIRE(model.series.size() == 1);
    CHECK(model.series.front().tag == "JSS");
}

TEST_CASE("selected unassigned sector keeps the full sentinel division") {
    auto analyticsRequest = request(AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                                    Breakdown::DivisionSectorPerson);
    analyticsRequest.sectors = {"Nao atribuido"};
    analyticsRequest.people = {"Nao atribuido"};

    const auto model = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, AnalyticsSeriesResult{}, AnalyticsChartMode::Custom);

    CHECK(model.categories == std::vector<std::string>{"Nao atribuido\nNao atribuido"});
    CHECK(model.series.front().name == "Nao atribuido");
    CHECK(model.series.front().values == std::vector<std::optional<double>>{0.0});
}

TEST_CASE("stock charts warn only when the latest capture is more than one week stale") {
    auto wholeRequest = request(AnalyticsMetric::Pending, TimeGrain::WholePeriod);
    AnalyticsSeriesResult stale{.points = {point("", "SMI", "SMIN", "", 4)},
                                .observedIsoYearWeek = 202610,
                                .observations = {{.bucketKey = "", .observedIsoYearWeek = 202610}}};

    const auto whole = ActivityAnalyticsChartModelBuilder::build(wholeRequest, stale,
                                                                 AnalyticsChartMode::SimpleBar);
    auto monthlyRequest = wholeRequest;
    monthlyRequest.grain = TimeGrain::IsoReferenceMonth;
    stale.points.front().bucketKey = "2026-03";
    stale.observations.front().bucketKey = "2026-03";
    const auto monthly = ActivityAnalyticsChartModelBuilder::build(monthlyRequest, stale,
                                                                   AnalyticsChartMode::LineTotal);
    stale.observedIsoYearWeek = 202612;
    stale.observations.front().observedIsoYearWeek = 202612;
    const auto oneWeekOld = ActivityAnalyticsChartModelBuilder::build(
        monthlyRequest, stale, AnalyticsChartMode::LineTotal);
    auto eventRequest = monthlyRequest;
    eventRequest.metric = AnalyticsMetric::Executed;
    stale.observedIsoYearWeek = 202601;
    const auto event = ActivityAnalyticsChartModelBuilder::build(eventRequest, stale,
                                                                 AnalyticsChartMode::LineTotal);

    CHECK(whole.qualityNote == "snapshot_stale_by_weeks=3");
    CHECK(monthly.qualityNote == "snapshot_stale_by_weeks=3");
    CHECK(oneWeekOld.qualityNote.empty());
    CHECK(event.qualityNote.empty());
}

TEST_CASE("deadline chart-only build composes stale and exclusion quality without warning input") {
    auto analyticsRequest = request(AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek);
    analyticsRequest.warningWindowDays.reset();
    auto included = point("202610", "SMI", "SMIN", "", 1);
    included.deadlineClass = DeadlineClass::OnTime;
    auto excluded = point("202610", "SMI", "SMIN", "", 2);
    excluded.deadlineClass = DeadlineClass::NotApplicableOrUnknown;
    const AnalyticsSeriesResult result{
        .points = {included, excluded},
        .observedIsoYearWeek = 202610,
        .excludedForDataQuality = 2,
        .observations = {{.bucketKey = "202610", .observedIsoYearWeek = 202610}}};

    const auto model = ActivityAnalyticsChartModelBuilder::build(
        analyticsRequest, result, AnalyticsChartMode::DeadlineStacked);
    const auto unavailable =
        ActivityAnalyticsChartModelBuilder::build(analyticsRequest,
                                                  {.observedIsoYearWeek = 202601,
                                                   .complete = false,
                                                   .unavailableReason = "projection_unavailable"},
                                                  AnalyticsChartMode::DeadlineStacked);

    CHECK(model.qualityNote == "excluded_for_data_quality=2 | snapshot_stale_by_weeks=3");
    CHECK(unavailable.qualityNote == "projection_unavailable");
}
