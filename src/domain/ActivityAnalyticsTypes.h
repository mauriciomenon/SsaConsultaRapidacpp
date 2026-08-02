#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    struct IsoWeek final {
        int year{0};
        int week{0};

        auto operator<=>(const IsoWeek&) const = default;
    };

    struct AnalyticsPeriod final {
        IsoWeek first;
        IsoWeek last;

        auto operator<=>(const AnalyticsPeriod&) const = default;
    };

    enum class AnalyticsMetric : std::uint8_t {
        Registered,
        Executed,
        PartialAttention,
        Spg,
        Apg,
        Apl,
        Pending,
        Issued,
        PendingDeadline,
    };

    enum class TimeGrain : std::uint8_t {
        WholePeriod,
        IsoWeek,
        IsoReferenceMonth,
    };

    enum class Breakdown : std::uint8_t {
        Division,
        DivisionSector,
        DivisionPerson,
        DivisionSectorPerson,
    };

    enum class PersonRole : std::uint8_t {
        Requester,
        Planner,
        Executor,
    };

    enum class RegistrationCohort : std::uint8_t {
        RegisteredInPeriod,
        RegisteredBeforePeriod,
        RegistrationUnknown,
    };

    enum class DeadlineClass : std::uint8_t {
        OnTime,
        Warning,
        Overdue,
        NotApplicableOrUnknown,
    };

    struct AnalyticsRequest final {
        AnalyticsMetric metric{AnalyticsMetric::Registered};
        AnalyticsPeriod period;
        TimeGrain grain{TimeGrain::WholePeriod};
        Breakdown breakdown{Breakdown::Division};
        PersonRole personRole{PersonRole::Executor};
        std::vector<std::string> divisions;
        std::vector<std::string> sectors;
        std::vector<std::string> people;
        std::optional<int> warningWindowDays;
    };

    struct AnalyticsPoint final {
        std::string bucketKey;
        std::string division;
        std::string sector;
        std::string person;
        RegistrationCohort cohort{RegistrationCohort::RegistrationUnknown};
        DeadlineClass deadlineClass{DeadlineClass::NotApplicableOrUnknown};
        std::int64_t count{0};
    };

    struct AnalyticsObservation final {
        std::string bucketKey;
        int observedIsoYearWeek{0};
        std::string sourceRevision;
    };

    struct AnalyticsSeriesResult final {
        std::vector<AnalyticsPoint> points;
        std::string sourceRevision;
        std::optional<int> observedIsoYearWeek;
        std::int64_t excludedForDataQuality{0};
        bool complete{true};
        std::string unavailableReason;
        std::vector<AnalyticsObservation> observations;

        [[nodiscard]] bool available() const noexcept {
            return complete && unavailableReason.empty();
        }
    };

    struct AnalyticsDimensionValues final {
        std::vector<std::string> divisions;
        std::vector<std::string> sectors;
        std::vector<std::string> people;
    };

    struct AnalyticsMetricAvailability final {
        AnalyticsMetric metric{AnalyticsMetric::Registered};
        std::optional<int> firstIsoYearWeek;
        std::optional<int> lastIsoYearWeek;
        bool available{false};
        std::string reason;
    };

    [[nodiscard]] bool isValidIsoWeek(IsoWeek value) noexcept;
    [[nodiscard]] std::optional<IsoWeek> isoWeekForDate(std::string_view isoDate) noexcept;
    [[nodiscard]] int toIsoYearWeek(IsoWeek value);
    [[nodiscard]] std::string isoReferenceMonth(IsoWeek value);

    struct IsoReferenceMonth final {
        int year{0};
        int month{0};

        [[nodiscard]] bool operator==(const IsoReferenceMonth&) const noexcept = default;
    };

    // Calendar year/month owning the ISO week, derived from the ISO Thursday. Callers
    // that need the numeric parts must use this instead of parsing isoReferenceMonth.
    [[nodiscard]] std::optional<IsoReferenceMonth> isoReferenceMonthParts(IsoWeek value) noexcept;
    [[nodiscard]] std::string analyticsBucketKey(IsoWeek value, TimeGrain grain);
    [[nodiscard]] std::string formatIsoYearWeekDisplay(int yearValue, int weekValue);
    [[nodiscard]] std::string formatAnalyticsBucketLabel(std::string_view bucketKey);
    [[nodiscard]] bool isValidPeriod(const AnalyticsPeriod& period) noexcept;
    struct YearToDateCalendarSelection final {
        int year{0};
        int month{0};
        IsoWeek first{};
        IsoWeek last{};
    };

    [[nodiscard]] AnalyticsPeriod calendarMonthPeriod(int yearValue, int monthValue);
    [[nodiscard]] AnalyticsPeriod isoReferenceMonthPeriod(int yearValue, int monthValue);
    [[nodiscard]] std::string personInitialsTag(std::string_view fullName);
    [[nodiscard]] std::string sectorCodeTag(std::string_view sectorCode);
    [[nodiscard]] std::string chartSeriesTag(std::string_view seriesName);
    [[nodiscard]] std::optional<YearToDateCalendarSelection>
    yearToDateCalendarSelection(std::string_view isoDate) noexcept;
    [[nodiscard]] AnalyticsPeriod referenceMonthHistoryPeriod(IsoWeek last, int monthCount);
    [[nodiscard]] std::vector<double> linearTrend(std::span<const double> values);
    [[nodiscard]] std::optional<std::string>
    validateAnalyticsRequest(const AnalyticsRequest& request);
    [[nodiscard]] RegistrationCohort registrationCohort(std::optional<int> registrationIsoYearWeek,
                                                        const AnalyticsPeriod& period) noexcept;
    [[nodiscard]] DeadlineClass classifyDeadline(std::string_view sourceState,
                                                 const std::optional<int>& deadlineOffsetDays,
                                                 int warningWindowDays) noexcept;

} // namespace ssa::domain
