#include "application/ActivityAnalyticsService.h"
#include "ports/IActivityAnalyticsSettingsPort.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace {

    using ssa::domain::AnalyticsMetric;
    using ssa::domain::AnalyticsMetricAvailability;
    using ssa::domain::AnalyticsPeriod;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::AnalyticsSeriesResult;
    using ssa::domain::Breakdown;
    using ssa::domain::PersonRole;
    using ssa::domain::TimeGrain;

    class RecordingAnalyticsPort final : public ssa::ports::IActivityAnalyticsPort {
      public:
        [[nodiscard]] AnalyticsSeriesResult series(const AnalyticsRequest& request,
                                                   std::stop_token stopToken) const override {
            static_cast<void>(stopToken);
            requests.push_back(request);
            if (cancelSource != nullptr && cancelAfterCalls.has_value() &&
                requests.size() == *cancelAfterCalls) {
                cancelSource->request_stop();
            }
            return {.sourceRevision = "series-" + std::to_string(requests.size())};
        }

        [[nodiscard]] ssa::domain::AnalyticsDimensionValues
        dimensionValues(const AnalyticsRequest& request, std::stop_token stopToken) const override {
            static_cast<void>(stopToken);
            dimensionRequests.push_back(request);
            return {.divisions = {"SMI"}, .sectors = {"SMIN-A"}, .people = {"Exec A"}};
        }

        [[nodiscard]] std::vector<AnalyticsMetricAvailability>
        availability(std::stop_token stopToken) const override {
            static_cast<void>(stopToken);
            ++availabilityCalls;
            return {{.metric = AnalyticsMetric::Registered, .available = true}};
        }

        mutable std::vector<AnalyticsRequest> requests;
        mutable std::vector<AnalyticsRequest> dimensionRequests;
        mutable int availabilityCalls{0};
        std::stop_source* cancelSource{nullptr};
        std::optional<std::size_t> cancelAfterCalls;
    };

    class RecordingAnalyticsSettingsPort final : public ssa::ports::IActivityAnalyticsSettingsPort {
      public:
        [[nodiscard]] std::optional<int>
        warningWindowDays(const std::stop_token stopToken) const override {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            ++readCalls;
            return value;
        }

        void setWarningWindowDays(const int days, const std::stop_token stopToken) override {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            ++writeCalls;
            value = days;
        }

        mutable int readCalls{0};
        int writeCalls{0};
        std::optional<int> value;
    };

    AnalyticsPeriod period() {
        return {{2026, 1}, {2026, 13}};
    }

    AnalyticsPeriod historyPeriod() {
        return {{2025, 14}, {2026, 13}};
    }

    AnalyticsRequest validRequest() {
        return {.metric = AnalyticsMetric::Registered,
                .period = period(),
                .grain = TimeGrain::WholePeriod,
                .breakdown = Breakdown::Division,
                .personRole = PersonRole::Executor};
    }

    void checkRequest(const AnalyticsRequest& request, const AnalyticsPeriod& expectedPeriod,
                      const AnalyticsMetric metric, const TimeGrain grain,
                      const Breakdown breakdown) {
        CHECK(request.metric == metric);
        CHECK(request.period == expectedPeriod);
        CHECK(request.grain == grain);
        CHECK(request.breakdown == breakdown);
        CHECK(request.personRole == PersonRole::Executor);
        CHECK(request.divisions.empty());
        CHECK(request.sectors.empty());
        CHECK(request.people.empty());
    }

} // namespace

TEST_CASE("activity analytics service builds dashboard requests in stable order") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);

    const auto dashboard = service.loadDashboard(period(), historyPeriod(), 7);

    REQUIRE(port->requests.size() == 13);
    checkRequest(port->requests[0], period(), AnalyticsMetric::Registered, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[1], historyPeriod(), AnalyticsMetric::Registered,
                 TimeGrain::IsoReferenceMonth, Breakdown::Division);
    checkRequest(port->requests[2], period(), AnalyticsMetric::Executed, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[3], historyPeriod(), AnalyticsMetric::Executed,
                 TimeGrain::IsoReferenceMonth, Breakdown::Division);
    checkRequest(port->requests[4], period(), AnalyticsMetric::PartialAttention,
                 TimeGrain::WholePeriod, Breakdown::DivisionSector);
    checkRequest(port->requests[5], period(), AnalyticsMetric::Spg, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[6], period(), AnalyticsMetric::Apg, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[7], period(), AnalyticsMetric::Apl, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[8], period(), AnalyticsMetric::Pending, TimeGrain::WholePeriod,
                 Breakdown::DivisionSector);
    checkRequest(port->requests[9], historyPeriod(), AnalyticsMetric::Pending,
                 TimeGrain::IsoReferenceMonth, Breakdown::Division);
    checkRequest(port->requests[10], period(), AnalyticsMetric::Issued, TimeGrain::WholePeriod,
                 Breakdown::Division);
    checkRequest(port->requests[11], historyPeriod(), AnalyticsMetric::Issued,
                 TimeGrain::IsoReferenceMonth, Breakdown::Division);
    checkRequest(port->requests[12], period(), AnalyticsMetric::PendingDeadline, TimeGrain::IsoWeek,
                 Breakdown::DivisionSector);
    for (std::size_t index = 0; index < 12; ++index) {
        CHECK_FALSE(port->requests[index].warningWindowDays.has_value());
    }
    CHECK(port->requests[12].warningWindowDays == 7);

    CHECK(dashboard.registeredBySector.sourceRevision == "series-1");
    CHECK(dashboard.registeredMonthly.sourceRevision == "series-2");
    CHECK(dashboard.executedBySector.sourceRevision == "series-3");
    CHECK(dashboard.executedMonthly.sourceRevision == "series-4");
    CHECK(dashboard.partialAttentionBySector.sourceRevision == "series-5");
    CHECK(dashboard.spgBySector.sourceRevision == "series-6");
    CHECK(dashboard.apgBySector.sourceRevision == "series-7");
    CHECK(dashboard.aplBySector.sourceRevision == "series-8");
    CHECK(dashboard.pendingBySector.sourceRevision == "series-9");
    CHECK(dashboard.pendingMonthly.sourceRevision == "series-10");
    CHECK(dashboard.issuedByDivision.sourceRevision == "series-11");
    CHECK(dashboard.issuedMonthly.sourceRevision == "series-12");
    CHECK(dashboard.pendingDeadlineWeekly.sourceRevision == "series-13");
}

TEST_CASE("activity analytics service reuses one deadline result for two charts") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);

    const auto dashboard = service.loadDashboard(period(), historyPeriod(), 14);

    CHECK(&dashboard.pendingDeadlinePercentage() == &dashboard.pendingDeadlineWeekly);
    CHECK(&dashboard.pendingDeadlineQuantity() == &dashboard.pendingDeadlineWeekly);
    CHECK(&dashboard.pendingDeadlinePercentage() == &dashboard.pendingDeadlineQuantity());
    CHECK(port->requests.size() == 13);
}

TEST_CASE("activity analytics service marks deadline unavailable without configuration") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);

    const auto dashboard = service.loadDashboard(period(), historyPeriod(), std::nullopt);

    REQUIRE(port->requests.size() == 12);
    CHECK(dashboard.pendingDeadlineWeekly.complete == false);
    CHECK_FALSE(dashboard.pendingDeadlineWeekly.available());
    CHECK(dashboard.pendingDeadlineWeekly.unavailableReason ==
          "warning window is required for deadline analytics");
}

TEST_CASE("activity analytics service cancels between dashboard requests") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    std::stop_source stopSource;
    port->cancelSource = &stopSource;
    port->cancelAfterCalls = 3;
    const ssa::application::ActivityAnalyticsService service(port);

    REQUIRE_THROWS_AS(service.loadDashboard(period(), historyPeriod(), 7, stopSource.get_token()),
                      std::system_error);
    CHECK(port->requests.size() == 3);
}

TEST_CASE("activity analytics service validates requests before using the port") {
    CHECK_THROWS_AS(ssa::application::ActivityAnalyticsService(nullptr), std::invalid_argument);

    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);
    auto request = validRequest();
    request.period.last = {2025, 52};
    CHECK_THROWS_AS(service.series(request), std::invalid_argument);
    CHECK(port->requests.empty());

    request = validRequest();
    request.period.last = {2025, 52};
    CHECK_THROWS_AS(service.dimensionValues(request), std::invalid_argument);
    CHECK(port->dimensionRequests.empty());

    request = validRequest();
    request.warningWindowDays = 366;
    CHECK_THROWS_AS(service.dimensionValues(request), std::invalid_argument);
    CHECK(port->dimensionRequests.empty());

    CHECK_THROWS_AS(service.loadDashboard(period(), historyPeriod(), 366), std::invalid_argument);
    auto invalidHistory = historyPeriod();
    invalidHistory.last = {2025, 13};
    CHECK_THROWS_AS(service.loadDashboard(period(), invalidHistory, 7), std::invalid_argument);
    CHECK(port->requests.empty());
}

TEST_CASE("activity analytics service loads person selector before people are selected") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);
    auto request = validRequest();
    request.metric = AnalyticsMetric::PendingDeadline;
    request.breakdown = Breakdown::DivisionSectorPerson;

    const auto dimensions = service.dimensionValues(request);

    CHECK(dimensions.people == std::vector<std::string>{"Exec A"});
    REQUIRE(port->dimensionRequests.size() == 1);
    CHECK(port->dimensionRequests.front().people.empty());
    CHECK_FALSE(port->dimensionRequests.front().warningWindowDays.has_value());
}

TEST_CASE("activity analytics service exposes port operations") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService service(port);
    const auto request = validRequest();

    CHECK(service.series(request).sourceRevision == "series-1");
    const auto dimensions = service.dimensionValues(request);
    CHECK(dimensions.divisions == std::vector<std::string>{"SMI"});
    CHECK(dimensions.sectors == std::vector<std::string>{"SMIN-A"});
    CHECK(dimensions.people == std::vector<std::string>{"Exec A"});
    const auto values = service.availability();
    REQUIRE(values.size() == 1);
    CHECK(values.front().metric == AnalyticsMetric::Registered);
    CHECK(values.front().available);
    CHECK(port->availabilityCalls == 1);
}

TEST_CASE("activity analytics service exposes persisted warning settings") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    auto settings = std::make_shared<RecordingAnalyticsSettingsPort>();
    const ssa::application::ActivityAnalyticsService service(port, settings);

    CHECK_FALSE(service.warningWindowDays().has_value());
    CHECK(settings->readCalls == 1);

    service.setWarningWindowDays(0);
    CHECK(settings->value == std::optional<int>{0});
    service.setWarningWindowDays(365);
    CHECK(service.warningWindowDays() == std::optional<int>{365});
    CHECK(settings->writeCalls == 2);

    CHECK_THROWS_AS(service.setWarningWindowDays(-1), std::invalid_argument);
    CHECK_THROWS_AS(service.setWarningWindowDays(366), std::invalid_argument);
    CHECK(settings->writeCalls == 2);
}

TEST_CASE("activity analytics service handles unavailable and canceled warning settings") {
    auto port = std::make_shared<RecordingAnalyticsPort>();
    const ssa::application::ActivityAnalyticsService withoutSettings(port);

    CHECK_FALSE(withoutSettings.warningWindowDays().has_value());
    CHECK_THROWS_AS(withoutSettings.setWarningWindowDays(7), std::logic_error);

    auto settings = std::make_shared<RecordingAnalyticsSettingsPort>();
    const ssa::application::ActivityAnalyticsService service(port, settings);
    std::stop_source stopSource;
    stopSource.request_stop();
    CHECK_THROWS_AS(service.warningWindowDays(stopSource.get_token()), std::system_error);
    CHECK_THROWS_AS(service.setWarningWindowDays(7, stopSource.get_token()), std::system_error);
    CHECK(settings->readCalls == 0);
    CHECK(settings->writeCalls == 0);
}
