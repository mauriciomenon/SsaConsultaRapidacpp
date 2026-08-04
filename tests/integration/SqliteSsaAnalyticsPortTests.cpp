#include "domain/ActivityAnalyticsTypes.h"
#include "infra/sqlite/SqliteActivityAnalyticsProjection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteSsaAnalyticsPort.h"
#include "ports/IActivityAnalyticsSettingsPort.h"
#include "ports/OperationError.h"
#include "query/ActivityAnalyticsSqlBuilder.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <stop_token>
#include <string>
#include <system_error>

namespace {

    using ssa::domain::AnalyticsMetric;
    using ssa::domain::AnalyticsMetricAvailability;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::AnalyticsSeriesResult;
    using ssa::domain::Breakdown;
    using ssa::domain::DeadlineClass;
    using ssa::domain::PersonRole;
    using ssa::domain::RegistrationCohort;
    using ssa::domain::TimeGrain;

    void execute(sqlite3* db, const std::string& sql) {
        char* error = nullptr;
        const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
        const std::string message = error == nullptr ? "" : error;
        sqlite3_free(error);
        INFO(message);
        REQUIRE(result == SQLITE_OK);
    }

    class AnalyticsFixture final {
      public:
        AnalyticsFixture() {
            REQUIRE(directory_.isValid());
            dbPath_ = std::filesystem::path{directory_.path().toStdString()} / "analytics.sqlite";
            sqlite3* db = nullptr;
            REQUIRE(sqlite3_open(dbPath_.string().c_str(), &db) == SQLITE_OK);
            execute(db,
                    "CREATE TABLE ssa_table("
                    "numero_ssa TEXT, situacao TEXT, setor_emissor TEXT, setor_executor TEXT, "
                    "semana_cadastro INTEGER, semana_executada INTEGER, solicitante TEXT, "
                    "responsavel_programacao TEXT, responsavel_execucao TEXT, prazo_limite TEXT, "
                    "status_execucao_prazo TEXT)");
            execute(db, "INSERT INTO ssa_table VALUES"
                        "('001','SPG','EEE-OPS','AAA-MECH',202552,202601,'Req A','Plan A','Exec A',"
                        "'2026-02-02','Dentro do Prazo'),"
                        "('002','APG','FFF-OPS','AAA-MECH',202601,202602,'Req B','Plan B','Exec B',"
                        "'2026-01-28','Fora de Prazo'),"
                        "('003','APL','EEE-OPS','BBB-ELEC',NULL,202602,'Req C','Plan C','',NULL,"
                        "'Nao Se Aplica'),"
                        "('004','SES','FFF-OPS','BBB-ELEC',202602,NULL,'Req D','Plan D','Exec D',"
                        "'2026-02-10','Dentro do Prazo'),"
                        "('005','ADM','EEE-OPS','BBB-ELEC',202603,202603,'Req E','Plan E','Exec E',"
                        "'2026-03-01','Dentro do Prazo')");
            REQUIRE(sqlite3_close(db) == SQLITE_OK);
        }

        [[nodiscard]] const std::filesystem::path& dbPath() const noexcept {
            return dbPath_;
        }

        void executeSql(const std::string& sql) const {
            sqlite3* db = nullptr;
            REQUIRE(sqlite3_open(dbPath_.string().c_str(), &db) == SQLITE_OK);
            execute(db, sql);
            REQUIRE(sqlite3_close(db) == SQLITE_OK);
        }

        void capture(const int isoYearWeek, std::string observedDate, std::string revision) const {
            sqlite3* db = nullptr;
            REQUIRE(sqlite3_open(dbPath_.string().c_str(), &db) == SQLITE_OK);
            const ssa::infra::sqlite::ActivityAnalyticsCaptureContext context{
                .observedIsoYearWeek = isoYearWeek,
                .observedDate = std::move(observedDate),
                .sourceRevision = revision,
                .sourceFingerprint = std::move(revision),
            };
            const auto result = ssa::infra::sqlite::SqliteActivityAnalyticsProjection::capture(
                db, "ssa_table", context);
            REQUIRE(result.changed);
            REQUIRE(sqlite3_close(db) == SQLITE_OK);
        }

        void captureHistory() const {
            capture(202605, "2026-01-29", "r1");
            executeSql("UPDATE ssa_table SET situacao='SES' WHERE numero_ssa='001'");
            capture(202606, "2026-02-05", "r2");
            executeSql("UPDATE ssa_table SET situacao='SES' WHERE numero_ssa='005'");
            capture(202609, "2026-02-26", "r3");
        }

      private:
        QTemporaryDir directory_;
        std::filesystem::path dbPath_;
    };

    AnalyticsRequest requestFor(const AnalyticsMetric metric) {
        AnalyticsRequest request;
        request.metric = metric;
        request.period = {{2026, 1}, {2026, 9}};
        request.breakdown = Breakdown::Division;
        request.warningWindowDays =
            metric == AnalyticsMetric::PendingDeadline ? std::optional<int>{7} : std::nullopt;
        return request;
    }

    std::int64_t total(const AnalyticsSeriesResult& result, const std::string& bucket = {}) {
        return std::accumulate(
            result.points.begin(), result.points.end(), std::int64_t{0},
            [&bucket](const std::int64_t sum, const ssa::domain::AnalyticsPoint& point) {
                return sum + (bucket.empty() || point.bucketKey == bucket ? point.count
                                                                          : std::int64_t{0});
            });
    }

    const AnalyticsMetricAvailability&
    availabilityFor(const std::vector<AnalyticsMetricAvailability>& values,
                    const AnalyticsMetric metric) {
        const auto found = std::ranges::find(values, metric, &AnalyticsMetricAvailability::metric);
        REQUIRE(found != values.end());
        return *found;
    }

    std::string explainQueryPlan(const std::filesystem::path& dbPath,
                                 const ssa::query::SqlQuery& query) {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
        sqlite3_stmt* statement = nullptr;
        const std::string explainSql = "EXPLAIN QUERY PLAN " + query.sql;
        INFO(explainSql);
        const int prepareResult =
            sqlite3_prepare_v2(db, explainSql.c_str(), -1, &statement, nullptr);
        INFO(sqlite3_errmsg(db));
        REQUIRE(prepareResult == SQLITE_OK);
        for (std::size_t index = 0; index < query.bindings.size(); ++index) {
            const auto& binding = query.bindings[index];
            REQUIRE(sqlite3_bind_text(statement, static_cast<int>(index + 1), binding.c_str(),
                                      static_cast<int>(binding.size()),
                                      SQLITE_TRANSIENT) == SQLITE_OK);
        }

        std::string plan;
        int result = SQLITE_ROW;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            if (!plan.empty()) {
                plan.push_back('\n');
            }
            const auto* detail = sqlite3_column_text(statement, 3);
            plan += detail == nullptr ? "" : reinterpret_cast<const char*>(detail);
        }
        const int finalizeResult = sqlite3_finalize(statement);
        const int closeResult = sqlite3_close(db);
        REQUIRE(result == SQLITE_DONE);
        REQUIRE(finalizeResult == SQLITE_OK);
        REQUIRE(closeResult == SQLITE_OK);
        return plan;
    }

} // namespace

TEST_CASE("analytics series and dimensions use selected week indexes") {
    AnalyticsFixture fixture;
    fixture.executeSql("CREATE INDEX idx_analytics_registration_week ON ssa_table(semana_cadastro);"
                       "CREATE INDEX idx_analytics_execution_week ON ssa_table(semana_executada)");
    const ssa::query::ActivityAnalyticsSqlBuilder builder;

    auto registeredRequest = requestFor(AnalyticsMetric::Registered);
    registeredRequest.period = {{2026, 1}, {2026, 3}};
    const auto registeredPlan =
        explainQueryPlan(fixture.dbPath(), builder.buildSeries(registeredRequest));
    INFO(registeredPlan);
    CHECK(registeredPlan.find("SEARCH ssa_table USING INDEX idx_analytics_registration_week") !=
          std::string::npos);

    auto executedRequest = requestFor(AnalyticsMetric::Executed);
    executedRequest.period = {{2026, 1}, {2026, 3}};
    const auto executedPlan =
        explainQueryPlan(fixture.dbPath(), builder.buildSeries(executedRequest));
    INFO(executedPlan);
    CHECK(executedPlan.find("SEARCH ssa_table USING INDEX idx_analytics_execution_week") !=
          std::string::npos);

    const auto dimensionPlan =
        explainQueryPlan(fixture.dbPath(), builder.buildDimensionValues(executedRequest).divisions);
    INFO(dimensionPlan);
    CHECK(dimensionPlan.find("SEARCH ssa_table USING INDEX idx_analytics_execution_week") !=
          std::string::npos);
}

TEST_CASE("analytics sqlite port executes events by executor person and cohort") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Executed);
    request.period = {{2026, 1}, {2026, 3}};
    request.grain = TimeGrain::IsoWeek;
    request.breakdown = Breakdown::DivisionSectorPerson;
    request.personRole = PersonRole::Executor;
    request.people = {"Exec A"};

    const auto result = port.series(request);

    REQUIRE(result.points.size() == 1);
    const auto& point = result.points.front();
    CHECK(point.bucketKey == "2026-W01");
    CHECK(point.division == "AAA");
    CHECK(point.sector == "AAA-MECH");
    CHECK(point.person == "Exec A");
    CHECK(point.cohort == RegistrationCohort::RegisteredBeforePeriod);
    CHECK(point.count == 1);
}

TEST_CASE("analytics sqlite port uses emitter dimensions for issued events") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Issued);
    request.period = {{2025, 52}, {2026, 3}};

    const auto result = port.series(request);

    REQUIRE(result.points.size() == 2);
    const auto eee =
        std::ranges::find(result.points, "EEE", &ssa::domain::AnalyticsPoint::division);
    const auto fff =
        std::ranges::find(result.points, "FFF", &ssa::domain::AnalyticsPoint::division);
    REQUIRE(eee != result.points.end());
    REQUIRE(fff != result.points.end());
    CHECK(eee->count == 2);
    CHECK(fff->count == 2);
}

TEST_CASE("analytics sqlite port selects the latest stock snapshot before period end") {
    AnalyticsFixture fixture;
    fixture.captureHistory();
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Pending);
    request.period = {{2026, 1}, {2026, 7}};

    const auto result = port.series(request);

    CHECK(result.available());
    CHECK(result.sourceRevision == "r2");
    CHECK(result.observedIsoYearWeek == 202606);
    REQUIRE(result.observations.size() == 1);
    CHECK(result.observations.front().bucketKey.empty());
    CHECK(result.observations.front().observedIsoYearWeek == 202606);
    CHECK(result.observations.front().sourceRevision == "r2");
    CHECK(total(result) == 3);
}

TEST_CASE("analytics sqlite port selects one final stock observation per month") {
    AnalyticsFixture fixture;
    fixture.captureHistory();
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Pending);
    request.period = {{2026, 5}, {2026, 9}};
    request.grain = TimeGrain::IsoReferenceMonth;

    const auto result = port.series(request);

    CHECK(result.sourceRevision == "r3");
    CHECK(result.observedIsoYearWeek == 202609);
    REQUIRE(result.observations.size() == 2);
    CHECK(result.observations[0].bucketKey == "2026-01");
    CHECK(result.observations[0].observedIsoYearWeek == 202605);
    CHECK(result.observations[0].sourceRevision == "r1");
    CHECK(result.observations[1].bucketKey == "2026-02");
    CHECK(result.observations[1].observedIsoYearWeek == 202609);
    CHECK(result.observations[1].sourceRevision == "r3");
    CHECK(total(result, "2026-01") == 4);
    CHECK(total(result, "2026-02") == 2);
    CHECK(total(result) == 6);
}

TEST_CASE("analytics sqlite port classifies deadline quality explicitly") {
    AnalyticsFixture fixture;
    fixture.capture(202605, "2026-01-29", "r1");
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::PendingDeadline);
    request.period = {{2026, 5}, {2026, 5}};

    const auto result = port.series(request);

    const auto countFor = [&result](const DeadlineClass value) {
        return std::accumulate(
            result.points.begin(), result.points.end(), std::int64_t{0},
            [value](const std::int64_t sum, const ssa::domain::AnalyticsPoint& point) {
                return sum + (point.deadlineClass == value ? point.count : 0);
            });
    };
    CHECK(countFor(DeadlineClass::OnTime) == 1);
    CHECK(countFor(DeadlineClass::Warning) == 1);
    CHECK(countFor(DeadlineClass::Overdue) == 1);
    CHECK(countFor(DeadlineClass::NotApplicableOrUnknown) == 1);
    CHECK(result.excludedForDataQuality == 1);
}

TEST_CASE("analytics sqlite port classifies a valid deadline without source state") {
    AnalyticsFixture fixture;
    fixture.executeSql("UPDATE ssa_table SET status_execucao_prazo='' WHERE numero_ssa='001'");
    fixture.capture(202605, "2026-01-29", "r1");
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::PendingDeadline);
    request.period = {{2026, 5}, {2026, 5}};

    const auto result = port.series(request);

    const auto warning = std::ranges::find(result.points, DeadlineClass::Warning,
                                           &ssa::domain::AnalyticsPoint::deadlineClass);
    REQUIRE(warning != result.points.end());
    CHECK(warning->count == 1);
    CHECK(result.excludedForDataQuality == 1);
}

TEST_CASE("analytics sqlite port excludes an invalid ISO week 53 event") {
    AnalyticsFixture fixture;
    fixture.executeSql(
        "INSERT INTO ssa_table VALUES('006','SES','EEE-OPS','AAA-MECH',202152,202153,"
        "'Req Invalid','Plan Invalid','Exec Invalid',NULL,'Nao Se Aplica')");
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Executed);
    request.period = {{2021, 52}, {2022, 1}};
    request.grain = TimeGrain::IsoWeek;
    request.breakdown = Breakdown::DivisionPerson;
    request.people = {"Exec Invalid"};

    const auto result = port.series(request);

    CHECK(result.points.empty());
}

TEST_CASE("analytics sqlite port cascades dimension values") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    auto request = requestFor(AnalyticsMetric::Executed);
    request.period = {{2026, 1}, {2026, 1}};
    request.breakdown = Breakdown::DivisionSectorPerson;
    request.personRole = PersonRole::Executor;

    const auto values = port.dimensionValues(request);

    CHECK(values.divisions == std::vector<std::string>{"AAA"});
    CHECK(values.sectors == std::vector<std::string>{"AAA-MECH"});
    CHECK(values.people == std::vector<std::string>{"Exec A"});
}

TEST_CASE("analytics sqlite port rejects stock dimensions without projection schema") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());

    CHECK_THROWS_AS(port.dimensionValues(requestFor(AnalyticsMetric::Pending)),
                    ssa::ports::OperationError);
}

TEST_CASE("analytics sqlite port reports live and projected availability") {
    AnalyticsFixture fixture;
    fixture.captureHistory();
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());

    const auto values = port.availability();

    REQUIRE(values.size() == 9);
    const auto& executed = availabilityFor(values, AnalyticsMetric::Executed);
    CHECK(executed.available);
    CHECK(executed.firstIsoYearWeek == 202601);
    CHECK(executed.lastIsoYearWeek == 202603);
    const auto& pending = availabilityFor(values, AnalyticsMetric::Pending);
    CHECK(pending.available);
    CHECK(pending.firstIsoYearWeek == 202605);
    CHECK(pending.lastIsoYearWeek == 202609);
    const auto& partial = availabilityFor(values, AnalyticsMetric::PartialAttention);
    CHECK_FALSE(partial.available);
    CHECK_FALSE(partial.reason.empty());
}

TEST_CASE("analytics sqlite port keeps events available without projection schema") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());

    const auto values = port.availability();
    CHECK(availabilityFor(values, AnalyticsMetric::Registered).available);
    CHECK_FALSE(availabilityFor(values, AnalyticsMetric::Pending).available);

    const auto result = port.series(requestFor(AnalyticsMetric::Pending));
    CHECK_FALSE(result.available());
    CHECK(result.points.empty());
    CHECK_FALSE(result.unavailableReason.empty());
}

TEST_CASE("analytics sqlite port requires real SSA numbers for event availability") {
    AnalyticsFixture fixture;
    fixture.executeSql("UPDATE ssa_table SET numero_ssa='  '");
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());

    const auto values = port.availability();

    CHECK_FALSE(availabilityFor(values, AnalyticsMetric::Issued).available);
    CHECK_FALSE(availabilityFor(values, AnalyticsMetric::Registered).available);
    CHECK_FALSE(availabilityFor(values, AnalyticsMetric::Executed).available);
}

TEST_CASE("analytics sqlite port honors pre-canceled reads") {
    const AnalyticsFixture fixture;
    const ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    std::stop_source stopSource;
    stopSource.request_stop();

    CHECK_THROWS_AS(port.series(requestFor(AnalyticsMetric::Registered), stopSource.get_token()),
                    std::system_error);
    CHECK_THROWS_AS(
        port.dimensionValues(requestFor(AnalyticsMetric::Registered), stopSource.get_token()),
        std::system_error);
    CHECK_THROWS_AS(port.availability(stopSource.get_token()), std::system_error);
}

TEST_CASE("analytics sqlite settings do not invent a warning window") {
    const AnalyticsFixture fixture;
    ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    const ssa::ports::IActivityAnalyticsSettingsPort& settings = port;

    CHECK_FALSE(settings.warningWindowDays().has_value());

    fixture.capture(202605, "2026-01-29", "r1");
    CHECK_FALSE(settings.warningWindowDays().has_value());
}

TEST_CASE("analytics sqlite settings persist valid boundary values across reopen") {
    const AnalyticsFixture fixture;
    fixture.capture(202605, "2026-01-29", "r1");
    {
        ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
        ssa::ports::IActivityAnalyticsSettingsPort& settings = port;
        settings.setWarningWindowDays(0);
        CHECK(settings.warningWindowDays() == std::optional<int>{0});
        settings.setWarningWindowDays(365);
    }

    const ssa::infra::sqlite::SqliteSsaAnalyticsPort reopened(fixture.dbPath());
    CHECK(reopened.warningWindowDays() == std::optional<int>{365});
}

TEST_CASE("analytics sqlite settings reject invalid values without mutation") {
    const AnalyticsFixture fixture;
    fixture.capture(202605, "2026-01-29", "r1");
    ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    port.setWarningWindowDays(21);

    CHECK_THROWS_AS(port.setWarningWindowDays(-1), std::invalid_argument);
    CHECK_THROWS_AS(port.setWarningWindowDays(366), std::invalid_argument);

    const ssa::infra::sqlite::SqliteSsaAnalyticsPort reopened(fixture.dbPath());
    CHECK(reopened.warningWindowDays() == std::optional<int>{21});
}

TEST_CASE("analytics sqlite settings honor pre-canceled reads and writes") {
    const AnalyticsFixture fixture;
    fixture.capture(202605, "2026-01-29", "r1");
    ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    std::stop_source stopSource;
    stopSource.request_stop();

    CHECK_THROWS_AS(port.warningWindowDays(stopSource.get_token()), std::system_error);
    CHECK_THROWS_AS(port.setWarningWindowDays(14, stopSource.get_token()), std::system_error);
    CHECK_FALSE(port.warningWindowDays().has_value());
}

TEST_CASE("analytics sqlite settings require the database write lock") {
    const AnalyticsFixture fixture;
    fixture.capture(202605, "2026-01-29", "r1");
    ssa::infra::sqlite::SqliteSsaAnalyticsPort port(fixture.dbPath());
    const ssa::infra::sqlite::SqliteDatabaseWriteLock heldLock(fixture.dbPath());
    REQUIRE(heldLock.acquired());

    CHECK_THROWS_AS(port.setWarningWindowDays(14), ssa::ports::OperationError);
    CHECK_FALSE(port.warningWindowDays().has_value());
}
