#include "domain/ActivityAnalyticsTypes.h"

#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ssa::domain {

    namespace {

        std::optional<std::chrono::sys_days> isoThursday(const IsoWeek value) noexcept {
            using namespace std::chrono;
            if (value.year < 1900 || value.year > 2999 || value.week < 1 || value.week > 53) {
                return std::nullopt;
            }
            const auto januaryFourth = sys_days{year{value.year} / January / 4};
            const auto firstMonday =
                januaryFourth - days{weekday{januaryFourth}.iso_encoding() - 1};
            const auto thursday = firstMonday + weeks{value.week - 1} + days{3};
            const year_month_day date{thursday};
            if (static_cast<int>(date.year()) != value.year) {
                return std::nullopt;
            }
            return thursday;
        }

        bool usesPeople(const Breakdown breakdown) noexcept {
            return breakdown == Breakdown::DivisionPerson ||
                   breakdown == Breakdown::DivisionSectorPerson;
        }

        std::string_view normalizedDeadlineState(std::string_view value) noexcept {
            while (!value.empty() && value.front() == ' ') {
                value.remove_prefix(1);
            }
            while (!value.empty() && value.back() == ' ') {
                value.remove_suffix(1);
            }
            return value;
        }

        bool equalsAsciiCaseInsensitive(const std::string_view left,
                                        const std::string_view right) noexcept {
            if (left.size() != right.size()) {
                return false;
            }
            for (std::size_t index = 0; index < left.size(); ++index) {
                const char value = left[index] >= 'a' && left[index] <= 'z'
                                       ? static_cast<char>(left[index] - ('a' - 'A'))
                                       : left[index];
                if (value != right[index]) {
                    return false;
                }
            }
            return true;
        }

        IsoWeek isoWeekForThursday(const std::chrono::sys_days thursday) {
            using namespace std::chrono;
            const year_month_day date{thursday};
            const int isoYear = static_cast<int>(date.year());
            const auto januaryFourth = sys_days{year{isoYear} / January / 4};
            const auto firstMonday =
                januaryFourth - days{weekday{januaryFourth}.iso_encoding() - 1};
            const auto elapsedDays = (thursday - firstMonday).count();
            return {isoYear, static_cast<int>(elapsedDays / 7) + 1};
        }

    } // namespace

    bool isValidIsoWeek(const IsoWeek value) noexcept {
        return isoThursday(value).has_value();
    }

    std::optional<IsoWeek> isoWeekForDate(const std::string_view isoDate) noexcept {
        if (isoDate.size() != 10 || isoDate[4] != '-' || isoDate[7] != '-') {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < isoDate.size(); ++index) {
            if (index == 4 || index == 7) {
                continue;
            }
            if (isoDate[index] < '0' || isoDate[index] > '9') {
                return std::nullopt;
            }
        }
        const int yearValue = (isoDate[0] - '0') * 1000 + (isoDate[1] - '0') * 100 +
                              (isoDate[2] - '0') * 10 + (isoDate[3] - '0');
        const unsigned monthValue =
            static_cast<unsigned>((isoDate[5] - '0') * 10 + (isoDate[6] - '0'));
        const unsigned dayValue =
            static_cast<unsigned>((isoDate[8] - '0') * 10 + (isoDate[9] - '0'));
        if (yearValue < 1900 || yearValue > 2999) {
            return std::nullopt;
        }

        using namespace std::chrono;
        const year_month_day date{year{yearValue}, month{monthValue}, day{dayValue}};
        if (!date.ok()) {
            return std::nullopt;
        }
        const sys_days day{date};
        const auto thursday = day + days{4 - static_cast<int>(weekday{day}.iso_encoding())};
        const auto result = isoWeekForThursday(thursday);
        return isValidIsoWeek(result) ? std::optional<IsoWeek>{result} : std::nullopt;
    }

    int toIsoYearWeek(const IsoWeek value) {
        if (!isValidIsoWeek(value)) {
            throw std::invalid_argument("invalid ISO week");
        }
        return value.year * 100 + value.week;
    }

    std::string isoReferenceMonth(const IsoWeek value) {
        const auto thursday = isoThursday(value);
        if (!thursday.has_value()) {
            throw std::invalid_argument("invalid ISO week");
        }
        const std::chrono::year_month_day date{*thursday};
        const auto month = static_cast<unsigned>(date.month());
        return std::to_string(static_cast<int>(date.year())) + (month < 10 ? "-0" : "-") +
               std::to_string(month);
    }

    std::string analyticsBucketKey(const IsoWeek value, const TimeGrain grain) {
        if (grain == TimeGrain::WholePeriod) {
            return {};
        }
        if (grain == TimeGrain::IsoReferenceMonth) {
            return isoReferenceMonth(value);
        }
        if (!isValidIsoWeek(value)) {
            throw std::invalid_argument("invalid ISO week");
        }
        return std::to_string(value.year) + (value.week < 10 ? "-W0" : "-W") +
               std::to_string(value.week);
    }

    bool isValidPeriod(const AnalyticsPeriod& period) noexcept {
        if (!isValidIsoWeek(period.first) || !isValidIsoWeek(period.last)) {
            return false;
        }
        return period.first <= period.last;
    }

    AnalyticsPeriod referenceMonthHistoryPeriod(const IsoWeek last, const int monthCount) {
        using namespace std::chrono;
        const auto lastThursday = isoThursday(last);
        if (!lastThursday.has_value() || monthCount < 1) {
            throw std::invalid_argument("reference month history period is invalid");
        }
        const year_month_day lastDate{*lastThursday};
        const year_month lastMonth{lastDate.year(), lastDate.month()};
        const year_month firstMonth = lastMonth - months{monthCount - 1};
        const year_month_day firstDay{firstMonth / day{1}};
        if (!firstDay.ok() || static_cast<int>(firstDay.year()) < 1900 ||
            static_cast<int>(firstDay.year()) > 2999) {
            throw std::invalid_argument("reference month history period is outside range");
        }
        const sys_days firstDate{firstDay};
        const int daysUntilThursday =
            (4 - static_cast<int>(weekday{firstDate}.iso_encoding()) + 7) % 7;
        const auto firstThursday = firstDate + days{daysUntilThursday};
        return {.first = isoWeekForThursday(firstThursday), .last = last};
    }

    std::vector<double> linearTrend(const std::span<const double> values) {
        if (values.size() < 2) {
            return {};
        }
        const auto count = static_cast<double>(values.size());
        const double meanX = (count - 1.0) / 2.0;
        const double meanY = std::accumulate(values.begin(), values.end(), 0.0) / count;

        double numerator = 0.0;
        double denominator = 0.0;
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double centeredX = static_cast<double>(index) - meanX;
            numerator += centeredX * (values[index] - meanY);
            denominator += centeredX * centeredX;
        }
        const double slope = numerator / denominator;
        const double intercept = meanY - slope * meanX;

        std::vector<double> trend;
        trend.reserve(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            trend.push_back(intercept + slope * static_cast<double>(index));
        }
        return trend;
    }

    std::optional<std::string> validateAnalyticsRequest(const AnalyticsRequest& request) {
        if (!isValidPeriod(request.period)) {
            return "analytics period is invalid";
        }
        if (request.metric == AnalyticsMetric::PendingDeadline &&
            !request.warningWindowDays.has_value()) {
            return "warning window is required for deadline analytics";
        }
        if (request.warningWindowDays.has_value() &&
            (*request.warningWindowDays < 0 || *request.warningWindowDays > 365)) {
            return "warning window must be between 0 and 365 days";
        }
        if (usesPeople(request.breakdown) && request.people.empty()) {
            return "person breakdown requires an explicit selection";
        }
        return std::nullopt;
    }

    RegistrationCohort registrationCohort(const std::optional<int> registrationIsoYearWeek,
                                          const AnalyticsPeriod& period) noexcept {
        if (!registrationIsoYearWeek.has_value() || !isValidPeriod(period)) {
            return RegistrationCohort::RegistrationUnknown;
        }
        const int first = period.first.year * 100 + period.first.week;
        const int last = period.last.year * 100 + period.last.week;
        if (*registrationIsoYearWeek < first) {
            return RegistrationCohort::RegisteredBeforePeriod;
        }
        if (*registrationIsoYearWeek <= last) {
            return RegistrationCohort::RegisteredInPeriod;
        }
        return RegistrationCohort::RegistrationUnknown;
    }

    DeadlineClass classifyDeadline(const std::string_view sourceState,
                                   const std::optional<int>& deadlineOffsetDays,
                                   const int warningWindowDays) noexcept {
        const auto normalizedState = normalizedDeadlineState(sourceState);
        if (equalsAsciiCaseInsensitive(normalizedState, "FORA DE PRAZO")) {
            return DeadlineClass::Overdue;
        }
        if (equalsAsciiCaseInsensitive(normalizedState, "NAO SE APLICA") ||
            !deadlineOffsetDays.has_value() || warningWindowDays < 0 || warningWindowDays > 365) {
            return DeadlineClass::NotApplicableOrUnknown;
        }
        if (*deadlineOffsetDays < 0) {
            return DeadlineClass::Overdue;
        }
        if (*deadlineOffsetDays <= warningWindowDays) {
            return DeadlineClass::Warning;
        }
        return DeadlineClass::OnTime;
    }

} // namespace ssa::domain
