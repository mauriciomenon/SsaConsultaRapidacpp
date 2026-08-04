#include "domain/ActivityAnalyticsTypes.h"

#include <algorithm>
#include <cctype>
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

        IsoWeek isoWeekForDay(const std::chrono::sys_days value) {
            using namespace std::chrono;
            return isoWeekForThursday(value +
                                      days{4 - static_cast<int>(weekday{value}.iso_encoding())});
        }

        IsoWeek nextIsoWeek(const IsoWeek value) {
            const IsoWeek sameYear{value.year, value.week + 1};
            if (isValidIsoWeek(sameYear)) {
                return sameYear;
            }
            return {value.year + 1, 1};
        }

        std::string trimAscii(std::string_view value) {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
                value.remove_prefix(1);
            }
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
                value.remove_suffix(1);
            }
            return std::string{value};
        }

        std::vector<std::string_view> splitWhitespace(std::string_view value) {
            std::vector<std::string_view> tokens;
            std::size_t index = 0;
            while (index < value.size()) {
                while (index < value.size() &&
                       std::isspace(static_cast<unsigned char>(value[index])) != 0) {
                    ++index;
                }
                if (index >= value.size()) {
                    break;
                }
                const std::size_t start = index;
                while (index < value.size() &&
                       std::isspace(static_cast<unsigned char>(value[index])) == 0) {
                    ++index;
                }
                tokens.push_back(value.substr(start, index - start));
            }
            return tokens;
        }

        char firstAlphaUpper(std::string_view token) {
            for (const char character : token) {
                if ((character >= 'A' && character <= 'Z') ||
                    (character >= 'a' && character <= 'z')) {
                    return character >= 'a' && character <= 'z'
                               ? static_cast<char>(character - ('a' - 'A'))
                               : character;
                }
            }
            return '\0';
        }

        bool equalsIgnoreAsciiCase(std::string_view left, std::string_view right) noexcept {
            if (left.size() != right.size()) {
                return false;
            }
            for (std::size_t index = 0; index < left.size(); ++index) {
                const unsigned char a = static_cast<unsigned char>(left[index]);
                const unsigned char b = static_cast<unsigned char>(right[index]);
                if (std::tolower(a) != std::tolower(b)) {
                    return false;
                }
            }
            return true;
        }

        bool isKnownChartIdentity(std::string_view name) noexcept {
            static constexpr std::string_view known[] = {
                "total",
                "registered_in_period",
                "registered_before_period",
                "registration_unknown",
                "on_time",
                "warning",
                "overdue",
            };
            return std::ranges::any_of(known, [&](const std::string_view candidate) {
                return candidate == name || equalsIgnoreAsciiCase(candidate, name);
            });
        }

        constexpr int kMaxIsoWeeksPerCalendarMonth = 6;

        std::string formatIsoReferenceMonth(const int yearValue, const int monthValue) {
            return std::to_string(yearValue) + (monthValue < 10 ? "-0" : "-") +
                   std::to_string(monthValue);
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
        const auto result = isoWeekForDay(sys_days{date});
        return isValidIsoWeek(result) ? std::optional<IsoWeek>{result} : std::nullopt;
    }

    int toIsoYearWeek(const IsoWeek value) {
        if (!isValidIsoWeek(value)) {
            throw std::invalid_argument("invalid ISO week");
        }
        return value.year * 100 + value.week;
    }

    std::optional<IsoReferenceMonth> isoReferenceMonthParts(const IsoWeek value) noexcept {
        const auto thursday = isoThursday(value);
        if (!thursday.has_value()) {
            return std::nullopt;
        }
        const std::chrono::year_month_day date{*thursday};
        return IsoReferenceMonth{static_cast<int>(date.year()),
                                 static_cast<int>(static_cast<unsigned>(date.month()))};
    }

    std::string isoReferenceMonth(const IsoWeek value) {
        const auto parts = isoReferenceMonthParts(value);
        if (!parts.has_value()) {
            throw std::invalid_argument("invalid ISO week");
        }
        return formatIsoReferenceMonth(parts->year, parts->month);
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
        return formatIsoYearWeekDisplay(value.year, value.week);
    }

    std::string formatIsoYearWeekDisplay(const int yearValue, const int weekValue) {
        if (yearValue < 1900 || yearValue > 2999 || weekValue < 1 || weekValue > 53) {
            throw std::invalid_argument("ISO week display is outside analytics range");
        }
        return std::to_string(yearValue * 100 + weekValue);
    }

    std::string formatAnalyticsBucketLabel(const std::string_view bucketKey) {
        return std::string{bucketKey};
    }

    bool isValidPeriod(const AnalyticsPeriod& period) noexcept {
        if (!isValidIsoWeek(period.first) || !isValidIsoWeek(period.last)) {
            return false;
        }
        return period.first <= period.last;
    }

    std::optional<YearToDateCalendarSelection>
    yearToDateCalendarSelection(const std::string_view isoDate) noexcept {
        const auto currentWeek = isoWeekForDate(isoDate);
        if (!currentWeek.has_value()) {
            return std::nullopt;
        }
        const auto parts = isoReferenceMonthParts(*currentWeek);
        if (!parts.has_value() || parts->year < 1900 || parts->year > 2999) {
            return std::nullopt;
        }
        return YearToDateCalendarSelection{
            .year = parts->year,
            .month = parts->month,
            .first = {currentWeek->year, 1},
            .last = *currentWeek,
        };
    }

    AnalyticsPeriod calendarMonthPeriod(const int yearValue, const int monthValue) {
        using namespace std::chrono;
        if (yearValue < 1900 || yearValue > 2999 || monthValue < 1 || monthValue > 12) {
            throw std::invalid_argument("calendar month is outside analytics range");
        }
        const year_month month{year{yearValue},
                               std::chrono::month{static_cast<unsigned>(monthValue)}};
        const year_month_day firstDay{month / day{1}};
        const year_month_day lastDay{month / last};
        if (!firstDay.ok() || !lastDay.ok()) {
            throw std::invalid_argument("calendar month is invalid");
        }
        return {.first = isoWeekForDay(sys_days{firstDay}),
                .last = isoWeekForDay(sys_days{lastDay})};
    }

    AnalyticsPeriod isoReferenceMonthPeriod(const int yearValue, const int monthValue) {
        if (yearValue < 1900 || yearValue > 2999 || monthValue < 1 || monthValue > 12) {
            throw std::invalid_argument("ISO reference month is outside analytics range");
        }
        const auto searchWindow = calendarMonthPeriod(yearValue, monthValue);
        const IsoReferenceMonth target{yearValue, monthValue};
        std::optional<IsoWeek> first;
        std::optional<IsoWeek> last;
        auto week = searchWindow.first;
        // A calendar month spans at most six ISO weeks. The bound also stops the walk
        // when the window ends outside the supported ISO range (December of year 2999).
        for (int step = 0; step < kMaxIsoWeeksPerCalendarMonth; ++step) {
            const auto parts = isoReferenceMonthParts(week);
            if (!parts.has_value()) {
                break;
            }
            if (*parts == target) {
                if (!first.has_value()) {
                    first = week;
                }
                last = week;
            }
            if (week == searchWindow.last) {
                break;
            }
            week = nextIsoWeek(week);
        }
        if (!first.has_value() || !last.has_value()) {
            throw std::invalid_argument("ISO reference month has no ISO weeks");
        }
        return {.first = *first, .last = *last};
    }

    std::string personInitialsTag(const std::string_view fullName) {
        const std::string trimmed = trimAscii(fullName);
        const auto tokens = splitWhitespace(trimmed);
        if (tokens.empty()) {
            return {};
        }
        if (tokens.size() == 1) {
            const char initial = firstAlphaUpper(tokens.front());
            return initial != '\0' ? std::string(1, initial) : std::string{};
        }
        std::string tag;
        tag.reserve(4);
        const char firstInitial = firstAlphaUpper(tokens.front());
        if (firstInitial != '\0') {
            tag.push_back(firstInitial);
        }
        for (std::size_t index = 1; index < tokens.size() && tag.size() < 4; ++index) {
            const char initial = firstAlphaUpper(tokens[index]);
            if (initial != '\0') {
                tag.push_back(initial);
            }
        }
        return tag;
    }

    std::string sectorCodeTag(const std::string_view sectorCode) {
        const std::string trimmed = trimAscii(sectorCode);
        if (trimmed.empty()) {
            return {};
        }
        std::string tag;
        tag.reserve(4);
        for (const char character : trimmed) {
            if (tag.size() >= 4) {
                break;
            }
            if (character >= 'a' && character <= 'z') {
                tag.push_back(static_cast<char>(character - ('a' - 'A')));
            } else {
                tag.push_back(character);
            }
        }
        return tag;
    }

    std::string chartSeriesTag(const std::string_view seriesName) {
        if (isKnownChartIdentity(seriesName)) {
            return {};
        }
        std::string_view target = seriesName;
        if (const auto separator = seriesName.rfind(" / "); separator != std::string_view::npos) {
            target = seriesName.substr(separator + 3);
        }
        const bool looksLikePerson =
            target.find(' ') != std::string_view::npos ||
            (target == seriesName && seriesName.find(' ') != std::string_view::npos);
        if (looksLikePerson) {
            const auto name = target.find(' ') != std::string_view::npos ? target : seriesName;
            return personInitialsTag(name);
        }
        return sectorCodeTag(target);
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
