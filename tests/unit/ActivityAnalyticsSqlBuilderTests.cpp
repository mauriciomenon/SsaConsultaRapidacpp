#include "query/ActivityAnalyticsSqlBuilder.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

    using ssa::domain::AnalyticsMetric;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::Breakdown;
    using ssa::domain::PersonRole;
    using ssa::domain::TimeGrain;

    AnalyticsRequest requestFor(const AnalyticsMetric metric = AnalyticsMetric::Registered) {
        AnalyticsRequest request;
        request.metric = metric;
        request.period = {{2026, 1}, {2026, 5}};
        request.warningWindowDays =
            metric == AnalyticsMetric::PendingDeadline ? std::optional<int>{14} : std::nullopt;
        return request;
    }

    void checkContains(const std::string& text, const std::string& expected) {
        CAPTURE(text, expected);
        CHECK(text.find(expected) != std::string::npos);
    }

    void checkNotContains(const std::string& text, const std::string& unexpected) {
        CAPTURE(text, unexpected);
        CHECK(text.find(unexpected) == std::string::npos);
    }

    std::string groupByClause(const std::string& sql) {
        const auto start = sql.find(" GROUP BY ");
        const auto end = sql.find(" ORDER BY ", start);
        REQUIRE(start != std::string::npos);
        REQUIRE(end != std::string::npos);
        return sql.substr(start, end - start);
    }

} // namespace

TEST_CASE("activity analytics event metrics use their fixed source dimensions and weeks") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;

    const auto registered = builder.buildSeries(requestFor(AnalyticsMetric::Registered));
    checkContains(registered.sql, "FROM \"ssa_table\"");
    checkContains(registered.sql, "\"setor_executor\"");
    checkContains(registered.sql, "\"semana_cadastro\"");
    checkContains(registered.sql, "COUNT(DISTINCT");
    checkContains(registered.sql, "AS \"registration_cohort\"");
    checkContains(registered.sql, "NULL AS \"observed_iso_week\", '' AS \"source_revision\", "
                                  "COUNT(DISTINCT \"ssa_number\") AS \"count\"");
    CHECK(registered.bindings == std::vector<std::string>{"202601", "202605", "202601", "202605"});

    const auto issued = builder.buildSeries(requestFor(AnalyticsMetric::Issued));
    checkContains(issued.sql, "\"setor_emissor\"");
    checkContains(issued.sql, "\"semana_cadastro\"");
    checkNotContains(issued.sql, "\"setor_executor\"");
    CHECK(issued.bindings == std::vector<std::string>{"202601", "202605", "202601", "202605"});

    const auto executed = builder.buildSeries(requestFor(AnalyticsMetric::Executed));
    checkContains(executed.sql, "\"setor_executor\"");
    checkContains(executed.sql, "\"semana_executada\"");
    CHECK(executed.bindings == std::vector<std::string>{"202601", "202605", "202601", "202605"});
}

TEST_CASE("activity analytics event series preserves a compiled source predicate") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Executed);
    request.grain = TimeGrain::IsoWeek;
    request.breakdown = Breakdown::DivisionSectorPerson;
    request.people = {"Caio"};
    const ssa::query::SqlWhereClause sourceFilter{
        R"("situacao" = ? AND "descricao_ssa" LIKE ? ESCAPE '\')", {"APV", "%motor%"}};

    const auto query = builder.buildSeries(request, sourceFilter);

    checkContains(query.sql,
                  R"(BETWEEN ? AND ? AND ("situacao" = ? AND "descricao_ssa" LIKE ? ESCAPE '\'))");
    checkContains(query.sql, "'unknown' AS \"registration_cohort\"");
    CHECK(query.bindings == std::vector<std::string>{"202601", "202605", "APV", "%motor%", "Caio"});
}

TEST_CASE("activity analytics preserves the source revision of each stock snapshot") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const auto query = builder.buildSeries(requestFor(AnalyticsMetric::Pending));

    checkContains(query.sql, "snapshots.\"source_revision\"");
    checkContains(query.sql,
                  "MAX(\"observed_iso_week\") AS \"observed_iso_week\", "
                  "\"source_revision\" AS \"source_revision\", SUM(\"count\") AS \"count\"");
    checkContains(groupByClause(query.sql), "\"source_revision\"");
}

TEST_CASE("activity analytics supports all fixed breakdown projections") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor();

    request.breakdown = Breakdown::Division;
    auto query = builder.buildSeries(request);
    checkContains(query.sql, "'' AS \"sector\"");
    checkContains(query.sql, "'' AS \"person\"");

    request.breakdown = Breakdown::DivisionSector;
    query = builder.buildSeries(request);
    checkContains(query.sql, "\"sector\" AS \"sector\"");
    checkContains(query.sql, "'' AS \"person\"");

    request.breakdown = Breakdown::DivisionPerson;
    request.people = {"ANA"};
    query = builder.buildSeries(request);
    checkContains(query.sql, "'' AS \"sector\"");
    checkContains(query.sql, "\"person\" AS \"person\"");

    request.breakdown = Breakdown::DivisionSectorPerson;
    query = builder.buildSeries(request);
    checkContains(query.sql, "\"sector\" AS \"sector\"");
    checkContains(query.sql, "\"person\" AS \"person\"");
}

TEST_CASE("activity analytics groups only by dimensions selected in the breakdown") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor();

    request.breakdown = Breakdown::Division;
    auto grouping = groupByClause(builder.buildSeries(request).sql);
    checkNotContains(grouping, "\"sector\"");
    checkNotContains(grouping, "\"person\"");

    request.breakdown = Breakdown::DivisionSector;
    grouping = groupByClause(builder.buildSeries(request).sql);
    checkContains(grouping, "\"sector\"");
    checkNotContains(grouping, "\"person\"");

    request.breakdown = Breakdown::DivisionPerson;
    request.people = {"ANA"};
    grouping = groupByClause(builder.buildSeries(request).sql);
    checkNotContains(grouping, "\"sector\"");
    checkContains(grouping, "\"person\"");

    request.breakdown = Breakdown::DivisionSectorPerson;
    grouping = groupByClause(builder.buildSeries(request).sql);
    checkContains(grouping, "\"sector\"");
    checkContains(grouping, "\"person\"");
}

TEST_CASE("activity analytics maps person roles only from their enum") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor();
    request.breakdown = Breakdown::DivisionPerson;
    request.people = {"ANA"};

    const std::array roles{
        std::pair{PersonRole::Requester, std::string{"\"solicitante\""}},
        std::pair{PersonRole::Planner, std::string{"\"responsavel_programacao\""}},
        std::pair{PersonRole::Executor, std::string{"\"responsavel_execucao\""}},
    };
    for (const auto& [role, expectedColumn] : roles) {
        request.personRole = role;
        const auto query = builder.buildSeries(request);
        checkContains(query.sql, expectedColumn);
    }
}

TEST_CASE("activity analytics binds every selected value") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Executed);
    request.breakdown = Breakdown::DivisionSectorPerson;
    request.divisions = {"SMM", "SMIN"};
    request.sectors = {"SMM1"};
    request.people = {"ANA' OR 1=1 --", "BIA"};

    const auto query = builder.buildSeries(request);

    checkContains(query.sql, "\"division\" IN (?, ?)");
    checkContains(query.sql, "\"sector\" IN (?)");
    checkContains(query.sql, "\"person\" IN (?, ?)");
    checkNotContains(query.sql, "ANA' OR 1=1 --");
    CHECK(query.bindings == std::vector<std::string>{"202601", "202605", "202601", "202605", "SMM",
                                                     "SMIN", "SMM1", "ANA' OR 1=1 --", "BIA"});
}

TEST_CASE("activity analytics requires explicit people only when building a person series") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor();
    request.breakdown = Breakdown::DivisionPerson;

    CHECK_THROWS_AS(builder.buildSeries(request), std::invalid_argument);
    CHECK_NOTHROW(builder.buildDimensionValues(request));
}

TEST_CASE("activity analytics maps stock metrics to fixed snapshot keys") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const std::array metrics{
        std::pair{AnalyticsMetric::PartialAttention, std::string{"partial_attention"}},
        std::pair{AnalyticsMetric::Spg, std::string{"spg"}},
        std::pair{AnalyticsMetric::Apg, std::string{"apg"}},
        std::pair{AnalyticsMetric::Apl, std::string{"apl"}},
        std::pair{AnalyticsMetric::Pending, std::string{"pending"}},
        std::pair{AnalyticsMetric::PendingDeadline, std::string{"pending_deadline"}},
    };

    for (const auto& [metric, key] : metrics) {
        const auto query = builder.buildSeries(requestFor(metric));
        checkContains(query.sql, "activity_analytics_snapshot");
        checkContains(query.sql, "activity_analytics_point");
        checkNotContains(query.sql, "FROM \"ssa_table\"");
        CHECK(query.bindings.at(0) == "SSA");
        CHECK(query.bindings.at(1) == key);
        CHECK(query.bindings.at(2) == "202605");
    }
}

TEST_CASE("activity analytics classifies deadline snapshots with a bound warning window") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const auto query = builder.buildSeries(requestFor(AnalyticsMetric::PendingDeadline));

    checkContains(query.sql, "deadline_source_state");
    checkContains(query.sql, "deadline_offset_days");
    checkContains(query.sql, "AS \"deadline_class\"");
    checkNotContains(query.sql, "14");
    CHECK(query.bindings == std::vector<std::string>{"SSA", "pending_deadline", "202605",
                                                     "executor", "202601", "202605", "14", "14"});
}

TEST_CASE("activity analytics binds the selected person role for snapshot points") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Pending);
    request.personRole = PersonRole::Planner;

    const auto query = builder.buildSeries(request);

    checkContains(query.sql, "\"person_role\" = ?");
    CHECK(query.bindings.at(3) == "planner");
}

TEST_CASE("activity analytics emits stable bucket keys for every grain") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Executed);

    request.grain = TimeGrain::WholePeriod;
    checkContains(builder.buildSeries(request).sql, "'' AS \"bucket_key\"");

    request.grain = TimeGrain::IsoWeek;
    checkContains(builder.buildSeries(request).sql, "printf('%04d-W%02d'");

    request.grain = TimeGrain::IsoReferenceMonth;
    const auto monthly = builder.buildSeries(request);
    checkContains(monthly.sql, "strftime('%Y-%m'");
    checkContains(monthly.sql, "-01-04");
}

TEST_CASE("activity analytics dimension queries use the selected metric period") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Executed);
    request.breakdown = Breakdown::DivisionSectorPerson;
    request.personRole = PersonRole::Planner;
    request.divisions = {"SMM"};
    request.sectors = {"SMM1"};

    const auto queries = builder.buildDimensionValues(request);

    checkContains(queries.divisions.sql, "SELECT DISTINCT \"division\" AS \"value\"");
    checkContains(queries.divisions.sql, "\"setor_executor\"");
    checkContains(queries.divisions.sql, "\"semana_executada\" BETWEEN ? AND ?");
    CHECK(queries.divisions.bindings == std::vector<std::string>{"202601", "202605"});
    checkContains(queries.sectors.sql, "\"division\" IN (?)");
    checkContains(queries.sectors.sql, "\"setor_executor\"");
    checkContains(queries.sectors.sql, "\"semana_executada\" BETWEEN ? AND ?");
    CHECK(queries.sectors.bindings == std::vector<std::string>{"202601", "202605", "SMM"});
    checkContains(queries.people.sql, "\"responsavel_programacao\"");
    checkContains(queries.people.sql, "\"semana_executada\" BETWEEN ? AND ?");
    checkContains(queries.people.sql, "\"division\" IN (?)");
    checkContains(queries.people.sql, "\"sector\" IN (?)");
    CHECK(queries.people.bindings == std::vector<std::string>{"202601", "202605", "SMM", "SMM1"});
}

TEST_CASE("stock analytics exposes dimensions from the selected snapshots") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::Pending);
    request.divisions = {"IEE"};

    const auto queries = builder.buildDimensionValues(request);

    checkContains(queries.divisions.sql, "activity_analytics_snapshot");
    checkContains(queries.sectors.sql, "activity_analytics_point");
    CHECK(queries.divisions.bindings ==
          std::vector<std::string>{"SSA", "pending", "202605", "executor"});
    CHECK(queries.sectors.bindings ==
          std::vector<std::string>{"SSA", "pending", "202605", "executor", "IEE"});
    checkContains(queries.people.sql, "activity_analytics_snapshot");
}

TEST_CASE("activity analytics availability uses fixed event and snapshot metrics") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const auto query = builder.buildAvailability();

    checkContains(query.sql, "'registered'");
    checkContains(query.sql, "'executed'");
    checkContains(query.sql, "'issued'");
    checkContains(query.sql, "activity_analytics_snapshot");
    checkContains(query.sql, "'pending_deadline'");
    checkNotContains(query.sql, "SELECT *");
    CHECK(query.bindings == std::vector<std::string>{"SSA"});
}

TEST_CASE("activity analytics exposes event availability without snapshot schema") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const auto query = builder.buildEventAvailability();

    checkContains(query.sql, "'registered'");
    checkContains(query.sql, "'executed'");
    checkContains(query.sql, "'issued'");
    checkContains(query.sql, "FROM \"ssa_table\"");
    checkNotContains(query.sql, "activity_analytics_snapshot");
    CHECK(query.bindings.empty());
}

TEST_CASE("activity analytics exposes stock availability only from snapshot schema") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    const auto query = builder.buildStockAvailability();

    checkContains(query.sql, "activity_analytics_snapshot");
    checkContains(query.sql, "'partial_attention'");
    checkContains(query.sql, "'pending_deadline'");
    checkNotContains(query.sql, "FROM \"ssa_table\"");
    checkContains(query.sql, "MIN(CASE WHEN snapshots.\"complete\" = 1 THEN "
                             "snapshots.\"observed_iso_week\" END)");
    checkContains(query.sql, "MAX(CASE WHEN snapshots.\"complete\" = 1 THEN "
                             "snapshots.\"observed_iso_week\" END)");
    CHECK(query.bindings == std::vector<std::string>{"SSA"});
}

TEST_CASE("activity analytics snapshot metadata preserves incomplete partial attention") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    auto request = requestFor(AnalyticsMetric::PartialAttention);

    const auto partial = builder.buildSnapshotMetadata(request);
    checkContains(partial.sql, "\"bucket_key\"");
    checkContains(partial.sql, "\"observed_iso_week\"");
    checkContains(partial.sql, "\"source_revision\"");
    checkContains(partial.sql, "\"complete\"");
    checkContains(partial.sql, "\"reason\"");
    checkNotContains(partial.sql, "AND \"complete\" = 1");
    CHECK(partial.bindings == std::vector<std::string>{"SSA", "partial_attention", "202605"});

    request.metric = AnalyticsMetric::Pending;
    const auto pending = builder.buildSnapshotMetadata(request);
    checkContains(pending.sql, "AND \"complete\" = 1");
    CHECK(pending.bindings == std::vector<std::string>{"SSA", "pending", "202605"});
}

TEST_CASE("activity analytics snapshot metadata rejects event metrics") {
    const ssa::query::ActivityAnalyticsSqlBuilder builder;
    CHECK_THROWS_AS(builder.buildSnapshotMetadata(requestFor(AnalyticsMetric::Registered)),
                    std::invalid_argument);
}

TEST_CASE("activity analytics rejects an unsafe configured source table") {
    CHECK_THROWS_AS(ssa::query::ActivityAnalyticsSqlBuilder{"ssa_table; DROP TABLE ssa_table"},
                    std::invalid_argument);
}
