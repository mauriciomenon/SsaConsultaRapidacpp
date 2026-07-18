#include "application/ActivityAnalyticsService.h"

#include <stdexcept>
#include <system_error>
#include <utility>

namespace ssa::application {

    namespace {

        void throwIfCanceled(const std::stop_token& stopToken) {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "activity analytics service canceled");
            }
        }

        void validateRequest(const domain::AnalyticsRequest& request) {
            if (const auto error = domain::validateAnalyticsRequest(request)) {
                throw std::invalid_argument(*error);
            }
        }

        void validatePeriodAndWarning(const domain::AnalyticsPeriod& period,
                                      const std::optional<int> warningWindowDays) {
            if (!domain::isValidPeriod(period)) {
                throw std::invalid_argument("analytics period is invalid");
            }
            if (warningWindowDays.has_value() &&
                (*warningWindowDays < 0 || *warningWindowDays > 365)) {
                throw std::invalid_argument("warning window must be between 0 and 365 days");
            }
        }

        domain::AnalyticsRequest
        dashboardRequest(const domain::AnalyticsMetric metric,
                         const domain::AnalyticsPeriod& period, const domain::TimeGrain grain,
                         const domain::Breakdown breakdown,
                         const std::optional<int> warningWindowDays = std::nullopt) {
            return {.metric = metric,
                    .period = period,
                    .grain = grain,
                    .breakdown = breakdown,
                    .personRole = domain::PersonRole::Executor,
                    .warningWindowDays = warningWindowDays};
        }

    } // namespace

    const domain::AnalyticsSeriesResult&
    ActivityAnalyticsDashboard::pendingDeadlinePercentage() const noexcept {
        return pendingDeadlineWeekly;
    }

    const domain::AnalyticsSeriesResult&
    ActivityAnalyticsDashboard::pendingDeadlineQuantity() const noexcept {
        return pendingDeadlineWeekly;
    }

    ActivityAnalyticsService::ActivityAnalyticsService(
        std::shared_ptr<const ports::IActivityAnalyticsPort> port,
        std::shared_ptr<ports::IActivityAnalyticsSettingsPort> settingsPort)
        : port_(std::move(port)), settingsPort_(std::move(settingsPort)) {
        if (!port_) {
            throw std::invalid_argument("activity analytics port is required");
        }
    }

    domain::AnalyticsSeriesResult
    ActivityAnalyticsService::series(const domain::AnalyticsRequest& request,
                                     const std::stop_token& stopToken) const {
        validateRequest(request);
        throwIfCanceled(stopToken);
        auto result = port_->series(request, stopToken);
        throwIfCanceled(stopToken);
        return result;
    }

    domain::AnalyticsDimensionValues
    ActivityAnalyticsService::dimensionValues(const domain::AnalyticsRequest& request,
                                              const std::stop_token& stopToken) const {
        validatePeriodAndWarning(request.period, request.warningWindowDays);
        throwIfCanceled(stopToken);
        auto result = port_->dimensionValues(request, stopToken);
        throwIfCanceled(stopToken);
        return result;
    }

    std::vector<domain::AnalyticsMetricAvailability>
    ActivityAnalyticsService::availability(const std::stop_token& stopToken) const {
        throwIfCanceled(stopToken);
        auto result = port_->availability(stopToken);
        throwIfCanceled(stopToken);
        return result;
    }

    std::optional<int>
    ActivityAnalyticsService::warningWindowDays(const std::stop_token& stopToken) const {
        throwIfCanceled(stopToken);
        if (!settingsPort_) {
            return std::nullopt;
        }
        auto result = settingsPort_->warningWindowDays(stopToken);
        throwIfCanceled(stopToken);
        return result;
    }

    void ActivityAnalyticsService::setWarningWindowDays(const int days,
                                                        const std::stop_token& stopToken) const {
        if (days < 0 || days > 365) {
            throw std::invalid_argument("warning window must be between 0 and 365 days");
        }
        throwIfCanceled(stopToken);
        if (!settingsPort_) {
            throw std::logic_error("activity analytics settings port is unavailable");
        }
        settingsPort_->setWarningWindowDays(days, stopToken);
        throwIfCanceled(stopToken);
    }

    ActivityAnalyticsDashboard ActivityAnalyticsService::loadDashboard(
        const domain::AnalyticsPeriod& reportPeriod, const domain::AnalyticsPeriod& historyPeriod,
        const std::optional<int> warningWindowDays, const std::stop_token& stopToken) const {
        validatePeriodAndWarning(reportPeriod, warningWindowDays);
        validatePeriodAndWarning(historyPeriod, std::nullopt);

        ActivityAnalyticsDashboard dashboard;
        dashboard.registeredBySector = series(
            dashboardRequest(domain::AnalyticsMetric::Registered, reportPeriod,
                             domain::TimeGrain::WholePeriod, domain::Breakdown::DivisionSector),
            stopToken);
        dashboard.registeredMonthly = series(
            dashboardRequest(domain::AnalyticsMetric::Registered, historyPeriod,
                             domain::TimeGrain::IsoReferenceMonth, domain::Breakdown::Division),
            stopToken);
        dashboard.executedBySector = series(
            dashboardRequest(domain::AnalyticsMetric::Executed, reportPeriod,
                             domain::TimeGrain::WholePeriod, domain::Breakdown::DivisionSector),
            stopToken);
        dashboard.executedMonthly = series(
            dashboardRequest(domain::AnalyticsMetric::Executed, historyPeriod,
                             domain::TimeGrain::IsoReferenceMonth, domain::Breakdown::Division),
            stopToken);
        dashboard.partialAttentionBySector = series(
            dashboardRequest(domain::AnalyticsMetric::PartialAttention, reportPeriod,
                             domain::TimeGrain::WholePeriod, domain::Breakdown::DivisionSector),
            stopToken);
        dashboard.spgBySector = series(dashboardRequest(domain::AnalyticsMetric::Spg, reportPeriod,
                                                        domain::TimeGrain::WholePeriod,
                                                        domain::Breakdown::DivisionSector),
                                       stopToken);
        dashboard.apgBySector = series(dashboardRequest(domain::AnalyticsMetric::Apg, reportPeriod,
                                                        domain::TimeGrain::WholePeriod,
                                                        domain::Breakdown::DivisionSector),
                                       stopToken);
        dashboard.aplBySector = series(dashboardRequest(domain::AnalyticsMetric::Apl, reportPeriod,
                                                        domain::TimeGrain::WholePeriod,
                                                        domain::Breakdown::DivisionSector),
                                       stopToken);
        dashboard.pendingBySector = series(
            dashboardRequest(domain::AnalyticsMetric::Pending, reportPeriod,
                             domain::TimeGrain::WholePeriod, domain::Breakdown::DivisionSector),
            stopToken);
        dashboard.pendingMonthly = series(
            dashboardRequest(domain::AnalyticsMetric::Pending, historyPeriod,
                             domain::TimeGrain::IsoReferenceMonth, domain::Breakdown::Division),
            stopToken);
        dashboard.issuedByDivision =
            series(dashboardRequest(domain::AnalyticsMetric::Issued, reportPeriod,
                                    domain::TimeGrain::WholePeriod, domain::Breakdown::Division),
                   stopToken);
        dashboard.issuedMonthly = series(
            dashboardRequest(domain::AnalyticsMetric::Issued, historyPeriod,
                             domain::TimeGrain::IsoReferenceMonth, domain::Breakdown::Division),
            stopToken);
        if (warningWindowDays.has_value()) {
            dashboard.pendingDeadlineWeekly =
                series(dashboardRequest(domain::AnalyticsMetric::PendingDeadline, reportPeriod,
                                        domain::TimeGrain::IsoWeek,
                                        domain::Breakdown::DivisionSector, warningWindowDays),
                       stopToken);
        } else {
            dashboard.pendingDeadlineWeekly.complete = false;
            dashboard.pendingDeadlineWeekly.unavailableReason =
                "warning window is required for deadline analytics";
        }
        throwIfCanceled(stopToken);
        return dashboard;
    }

} // namespace ssa::application
