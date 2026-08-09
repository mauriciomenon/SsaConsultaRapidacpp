#include "application/ActivityAnalyticsChartModelBuilder.h"

#include "application/ActivityAnalyticsService.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ssa::application {

    namespace {

        using CountByCategory = std::map<std::string, std::int64_t>;

        constexpr std::array<std::string_view, 3> kCohortNames{
            "registered_in_period",
            "registered_before_period",
            "registration_unknown",
        };
        constexpr std::array<std::string_view, 3> kDeadlineNames{
            "on_time",
            "warning",
            "overdue",
        };

        std::string taggedSeriesName(const std::string_view name) {
            return domain::chartSeriesTag(name);
        }

        AnalyticsChartSeries makeSeries(const std::string_view name,
                                        std::vector<std::optional<double>> values,
                                        std::optional<double> total) {
            return {
                .name = std::string{name},
                .tag = taggedSeriesName(name),
                .values = std::move(values),
                .total = total,
            };
        }

        void checkedAdd(std::int64_t& destination, const std::int64_t value) {
            if (value < 0) {
                throw std::invalid_argument("analytics chart count cannot be negative");
            }
            if (destination > std::numeric_limits<std::int64_t>::max() - value) {
                throw std::overflow_error("analytics chart count overflow");
            }
            destination += value;
        }

        bool isEventMetric(const domain::AnalyticsMetric metric) noexcept {
            return metric == domain::AnalyticsMetric::Registered ||
                   metric == domain::AnalyticsMetric::Executed ||
                   metric == domain::AnalyticsMetric::Issued;
        }

        bool usesPeople(const domain::Breakdown breakdown) noexcept {
            return breakdown == domain::Breakdown::DivisionPerson ||
                   breakdown == domain::Breakdown::DivisionSectorPerson;
        }

        void validateChartRequest(const domain::AnalyticsRequest& request) {
            if (!domain::isValidPeriod(request.period)) {
                throw std::invalid_argument("analytics period is invalid");
            }
            if (request.warningWindowDays.has_value() &&
                (*request.warningWindowDays < 0 || *request.warningWindowDays > 365)) {
                throw std::invalid_argument("warning window must be between 0 and 365 days");
            }
            if (usesPeople(request.breakdown) && request.people.empty()) {
                throw std::invalid_argument("person breakdown requires an explicit selection");
            }
        }

        bool includedInChart(const domain::AnalyticsRequest& request,
                             const domain::AnalyticsPoint& point) noexcept {
            return request.metric != domain::AnalyticsMetric::PendingDeadline ||
                   point.deadlineClass != domain::DeadlineClass::NotApplicableOrUnknown;
        }

        bool stockDataIsKnown(const domain::AnalyticsRequest& request,
                              const domain::AnalyticsSeriesResult& result) noexcept {
            return !isEventMetric(request.metric) &&
                   (!result.points.empty() || !result.observations.empty() ||
                    result.observedIsoYearWeek.has_value());
        }

        std::string dimensionValue(const std::string& value) {
            return value.empty() ? "Nao atribuido" : value;
        }

        std::string selectedSectorDivision(const std::string& sector) {
            return sector.empty() || sector == "Nao atribuido" ? "Nao atribuido"
                                                               : sector.substr(0, 3);
        }

        std::string dimensionKey(const domain::AnalyticsRequest& request,
                                 const domain::AnalyticsPoint& point) {
            auto division = dimensionValue(point.division);
            switch (request.breakdown) {
            case domain::Breakdown::Division:
                return division;
            case domain::Breakdown::DivisionSector:
                return dimensionValue(point.sector);
            case domain::Breakdown::DivisionPerson:
                return division + " / " + dimensionValue(point.person);
            case domain::Breakdown::DivisionSectorPerson:
                return dimensionValue(point.sector) + " / " + dimensionValue(point.person);
            }
            throw std::invalid_argument("unknown analytics breakdown");
        }

        std::set<std::string> selectedDimensionKeys(const domain::AnalyticsRequest& request) {
            std::set<std::string> keys;
            domain::AnalyticsPoint selected;
            switch (request.breakdown) {
            case domain::Breakdown::Division:
                for (const auto& division : request.divisions) {
                    selected.division = division;
                    keys.insert(dimensionKey(request, selected));
                }
                break;
            case domain::Breakdown::DivisionSector:
                for (const auto& sector : request.sectors) {
                    selected.division = selectedSectorDivision(sector);
                    selected.sector = sector;
                    keys.insert(dimensionKey(request, selected));
                }
                break;
            case domain::Breakdown::DivisionPerson:
                for (const auto& division : request.divisions) {
                    selected.division = division;
                    for (const auto& person : request.people) {
                        selected.person = person;
                        keys.insert(dimensionKey(request, selected));
                    }
                }
                break;
            case domain::Breakdown::DivisionSectorPerson:
                for (const auto& sector : request.sectors) {
                    selected.division = selectedSectorDivision(sector);
                    selected.sector = sector;
                    for (const auto& person : request.people) {
                        selected.person = person;
                        keys.insert(dimensionKey(request, selected));
                    }
                }
                break;
            }
            return keys;
        }

        domain::IsoWeek nextIsoWeek(const domain::IsoWeek value) {
            const domain::IsoWeek sameYear{value.year, value.week + 1};
            if (domain::isValidIsoWeek(sameYear)) {
                return sameYear;
            }
            return {value.year + 1, 1};
        }

        std::string staleSnapshotQuality(const domain::AnalyticsRequest& request,
                                         const domain::AnalyticsSeriesResult& result) {
            if (isEventMetric(request.metric)) {
                return {};
            }
            auto latest = result.observedIsoYearWeek;
            for (const auto& observation : result.observations) {
                if (!latest.has_value() || observation.observedIsoYearWeek > *latest) {
                    latest = observation.observedIsoYearWeek;
                }
            }
            if (!latest.has_value()) {
                return {};
            }

            auto observed = domain::IsoWeek{*latest / 100, *latest % 100};
            if (!domain::isValidIsoWeek(observed)) {
                throw std::invalid_argument("observed snapshot has an invalid ISO week");
            }
            int weeksBehind = 0;
            while (observed < request.period.last) {
                observed = nextIsoWeek(observed);
                ++weeksBehind;
            }
            if (weeksBehind <= 1) {
                return {};
            }
            return "snapshot_stale_by_weeks=" + std::to_string(weeksBehind);
        }

        std::vector<std::string> temporalCategories(const domain::AnalyticsRequest& request) {
            std::vector<std::string> categories;
            std::string previous;
            auto week = request.period.first;
            while (week <= request.period.last) {
                auto key = domain::analyticsBucketKey(week, request.grain);
                if (key != previous) {
                    categories.push_back(key);
                    previous = std::move(key);
                }
                if (week == request.period.last) {
                    break;
                }
                week = nextIsoWeek(week);
            }
            return categories;
        }

        std::vector<std::string>
        wholePeriodCategories(const domain::AnalyticsRequest& request,
                              const domain::AnalyticsSeriesResult& result) {
            auto categories = selectedDimensionKeys(request);
            for (const auto& point : result.points) {
                categories.insert(dimensionKey(request, point));
            }
            if (categories.empty() && stockDataIsKnown(request, result)) {
                categories.insert("Total");
            }
            return {categories.begin(), categories.end()};
        }

        std::vector<std::string> categories(const domain::AnalyticsRequest& request,
                                            const domain::AnalyticsSeriesResult& result) {
            if (request.grain == domain::TimeGrain::WholePeriod) {
                return wholePeriodCategories(request, result);
            }
            return temporalCategories(request);
        }

        std::string axisKey(const domain::AnalyticsRequest& request,
                            const domain::AnalyticsPoint& point) {
            if (request.grain == domain::TimeGrain::WholePeriod) {
                return dimensionKey(request, point);
            }
            return point.bucketKey;
        }

        void requireKnownCategory(const std::set<std::string>& knownCategories,
                                  const std::string& key) {
            if (!knownCategories.contains(key)) {
                throw std::invalid_argument("analytics point is outside the chart period");
            }
        }

        bool totalIsKnown(const domain::AnalyticsRequest& request,
                          const domain::AnalyticsSeriesResult& result) noexcept {
            return isEventMetric(request.metric) || stockDataIsKnown(request, result);
        }

        void addObservedBuckets(const domain::AnalyticsRequest& request,
                                const domain::AnalyticsSeriesResult& result,
                                const std::set<std::string>& knownCategories,
                                std::set<std::string>& observedCategories) {
            if (request.grain == domain::TimeGrain::WholePeriod) {
                if (totalIsKnown(request, result)) {
                    observedCategories.insert(knownCategories.begin(), knownCategories.end());
                }
                return;
            }
            for (const auto& observation : result.observations) {
                requireKnownCategory(knownCategories, observation.bucketKey);
                observedCategories.insert(observation.bucketKey);
            }
        }

        std::optional<double> optionalTotal(const bool known, const std::int64_t value) {
            if (!known) {
                return std::nullopt;
            }
            return static_cast<double>(value);
        }

        std::optional<double> missingValue(const domain::AnalyticsRequest& request,
                                           const bool categoryObserved) {
            if (isEventMetric(request.metric) || categoryObserved) {
                return 0.0;
            }
            return std::nullopt;
        }

        std::vector<std::optional<double>>
        valuesFor(const domain::AnalyticsRequest& request,
                  const std::vector<std::string>& allCategories, const CountByCategory& counts,
                  const std::set<std::string>& observedCategories) {
            std::vector<std::optional<double>> values;
            values.reserve(allCategories.size());
            for (const auto& category : allCategories) {
                const auto found = counts.find(category);
                if (found != counts.end()) {
                    values.emplace_back(static_cast<double>(found->second));
                } else {
                    values.push_back(missingValue(request, observedCategories.contains(category)));
                }
            }
            return values;
        }

        std::vector<std::optional<double>>
        trendFor(const std::vector<std::optional<double>>& values) {
            std::size_t observedCount = 0;
            double sumX = 0.0;
            double sumY = 0.0;
            for (std::size_t index = 0; index < values.size(); ++index) {
                const auto& value = values[index];
                if (value.has_value()) {
                    ++observedCount;
                    sumX += static_cast<double>(index);
                    sumY += value.value_or(0.0);
                }
            }
            if (observedCount < 2) {
                return {};
            }

            const double meanX = sumX / static_cast<double>(observedCount);
            const double meanY = sumY / static_cast<double>(observedCount);
            double numerator = 0.0;
            double denominator = 0.0;
            for (std::size_t index = 0; index < values.size(); ++index) {
                const auto& value = values[index];
                if (!value.has_value()) {
                    continue;
                }
                const double centeredX = static_cast<double>(index) - meanX;
                numerator += centeredX * (value.value_or(0.0) - meanY);
                denominator += centeredX * centeredX;
            }
            if (denominator == 0.0) {
                return {};
            }

            const double slope = numerator / denominator;
            const double intercept = meanY - slope * meanX;
            std::vector<std::optional<double>> trend;
            trend.reserve(values.size());
            for (std::size_t index = 0; index < values.size(); ++index) {
                trend.emplace_back(intercept + slope * static_cast<double>(index));
            }
            return trend;
        }

        std::string observedSubtitle(const domain::AnalyticsSeriesResult& result) {
            std::string subtitle;
            if (result.observedIsoYearWeek.has_value()) {
                const int value = *result.observedIsoYearWeek;
                subtitle = "Semana observada: " +
                           domain::formatIsoYearWeekDisplay(value / 100, value % 100);
            }
            if (!result.sourceRevision.empty()) {
                if (!subtitle.empty()) {
                    subtitle += " | ";
                }
                subtitle += "Revisao: " + result.sourceRevision;
            }
            return subtitle;
        }

        std::int64_t validateDeadlineQuality(const domain::AnalyticsRequest& request,
                                             const domain::AnalyticsSeriesResult& result) {
            if (request.metric != domain::AnalyticsMetric::PendingDeadline) {
                if (result.excludedForDataQuality != 0) {
                    throw std::logic_error(
                        "quality exclusions are only valid for deadline analytics");
                }
                return 0;
            }

            std::int64_t excluded = 0;
            for (const auto& point : result.points) {
                if (point.deadlineClass == domain::DeadlineClass::NotApplicableOrUnknown) {
                    checkedAdd(excluded, point.count);
                }
            }
            if (excluded != result.excludedForDataQuality) {
                throw std::logic_error("deadline quality exclusion count does not reconcile");
            }
            return excluded;
        }

        std::size_t cohortIndex(const domain::RegistrationCohort cohort) {
            switch (cohort) {
            case domain::RegistrationCohort::RegisteredInPeriod:
                return 0;
            case domain::RegistrationCohort::RegisteredBeforePeriod:
                return 1;
            case domain::RegistrationCohort::RegistrationUnknown:
                return 2;
            }
            throw std::invalid_argument("unknown analytics registration cohort");
        }

        std::size_t deadlineIndex(const domain::DeadlineClass deadlineClass) {
            switch (deadlineClass) {
            case domain::DeadlineClass::OnTime:
                return 0;
            case domain::DeadlineClass::Warning:
                return 1;
            case domain::DeadlineClass::Overdue:
                return 2;
            case domain::DeadlineClass::NotApplicableOrUnknown:
                break;
            }
            throw std::invalid_argument("excluded deadline class has no chart series");
        }

        AnalyticsChartModel buildSingleSeries(const domain::AnalyticsRequest& request,
                                              const domain::AnalyticsSeriesResult& result,
                                              const bool includeTrend) {
            AnalyticsChartModel model;
            model.categories = categories(request, result);
            const std::set<std::string> knownCategories(model.categories.begin(),
                                                        model.categories.end());
            std::set<std::string> observedCategories;
            addObservedBuckets(request, result, knownCategories, observedCategories);
            CountByCategory counts;
            std::int64_t total = 0;
            for (const auto& point : result.points) {
                if (!includedInChart(request, point)) {
                    continue;
                }
                const auto key = axisKey(request, point);
                requireKnownCategory(knownCategories, key);
                observedCategories.insert(key);
                checkedAdd(counts[key], point.count);
                checkedAdd(total, point.count);
            }

            AnalyticsChartSeries series{
                .name = "total",
                .tag = taggedSeriesName("total"),
                .values = valuesFor(request, model.categories, counts, observedCategories),
                .total = optionalTotal(totalIsKnown(request, result), total),
            };
            if (includeTrend) {
                series.trendValues = trendFor(series.values);
            }
            model.series.push_back(std::move(series));
            model.total = optionalTotal(totalIsKnown(request, result), total);
            return model;
        }

        AnalyticsChartModel buildCohortStack(const domain::AnalyticsRequest& request,
                                             const domain::AnalyticsSeriesResult& result) {
            AnalyticsChartModel model;
            model.categories = categories(request, result);
            const std::set<std::string> knownCategories(model.categories.begin(),
                                                        model.categories.end());
            std::set<std::string> observedCategories;
            addObservedBuckets(request, result, knownCategories, observedCategories);
            std::array<CountByCategory, 3> counts;
            std::array<std::int64_t, 3> totals{};
            std::int64_t total = 0;
            for (const auto& point : result.points) {
                const auto key = axisKey(request, point);
                requireKnownCategory(knownCategories, key);
                observedCategories.insert(key);
                const auto index = cohortIndex(point.cohort);
                checkedAdd(counts[index][key], point.count);
                checkedAdd(totals[index], point.count);
                checkedAdd(total, point.count);
            }

            const bool known = totalIsKnown(request, result);
            for (std::size_t index = 0; index < counts.size(); ++index) {
                model.series.push_back(makeSeries(
                    kCohortNames[index],
                    valuesFor(request, model.categories, counts[index], observedCategories),
                    optionalTotal(known, totals[index])));
            }
            model.total = optionalTotal(known, total);
            return model;
        }

        AnalyticsChartModel buildDeadlineStack(const domain::AnalyticsRequest& request,
                                               const domain::AnalyticsSeriesResult& result) {
            AnalyticsChartModel model;
            model.categories = categories(request, result);
            const std::set<std::string> knownCategories(model.categories.begin(),
                                                        model.categories.end());
            std::set<std::string> observedCategories;
            addObservedBuckets(request, result, knownCategories, observedCategories);
            std::array<CountByCategory, 3> counts;
            std::array<std::int64_t, 3> totals{};
            std::int64_t total = 0;
            for (const auto& point : result.points) {
                const auto key = axisKey(request, point);
                requireKnownCategory(knownCategories, key);
                observedCategories.insert(key);
                if (!includedInChart(request, point)) {
                    continue;
                }
                const auto index = deadlineIndex(point.deadlineClass);
                checkedAdd(counts[index][key], point.count);
                checkedAdd(totals[index], point.count);
                checkedAdd(total, point.count);
            }

            const bool known = totalIsKnown(request, result);
            for (std::size_t index = 0; index < counts.size(); ++index) {
                model.series.push_back(makeSeries(
                    kDeadlineNames[index],
                    valuesFor(request, model.categories, counts[index], observedCategories),
                    optionalTotal(known, totals[index])));
            }
            model.total = optionalTotal(known, total);
            return model;
        }

        void normalizeDeadlinePercentages(AnalyticsChartModel& model) {
            for (std::size_t category = 0; category < model.categories.size(); ++category) {
                double denominator = 0.0;
                std::optional<std::size_t> lastObservedSeries;
                for (std::size_t series = 0; series < model.series.size(); ++series) {
                    const auto& value = model.series[series].values[category];
                    if (!value.has_value()) {
                        continue;
                    }
                    denominator += *value;
                    lastObservedSeries = series;
                }
                if (denominator <= 0.0 || !lastObservedSeries.has_value()) {
                    continue;
                }

                double accumulated = 0.0;
                for (std::size_t series = 0; series < model.series.size(); ++series) {
                    auto& value = model.series[series].values[category];
                    if (!value.has_value()) {
                        continue;
                    }
                    const double percentage = series == *lastObservedSeries
                                                  ? 100.0 - accumulated
                                                  : *value * 100.0 / denominator;
                    value = percentage;
                    accumulated += percentage;
                }
            }
        }

        AnalyticsChartModel buildCustom(const domain::AnalyticsRequest& request,
                                        const domain::AnalyticsSeriesResult& result) {
            if (request.grain == domain::TimeGrain::WholePeriod) {
                if (request.breakdown == domain::Breakdown::DivisionSectorPerson) {
                    AnalyticsChartModel model;
                    std::set<std::string> categorySet;
                    std::map<std::string, CountByCategory> counts;
                    std::map<std::string, std::int64_t> totals;
                    const auto category = [](const std::string& sector) {
                        return dimensionValue(sector);
                    };
                    for (const auto& sector : request.sectors) {
                        categorySet.insert(category(sector));
                    }
                    for (const auto& person : request.people) {
                        const auto name = dimensionValue(person);
                        counts.try_emplace(name);
                        totals.try_emplace(name, 0);
                    }
                    for (const auto& point : result.points) {
                        categorySet.insert(category(point.sector));
                    }
                    model.categories.assign(categorySet.begin(), categorySet.end());

                    const std::set<std::string> knownCategories(model.categories.begin(),
                                                                model.categories.end());
                    std::set<std::string> observedCategories;
                    addObservedBuckets(request, result, knownCategories, observedCategories);
                    std::int64_t total = 0;
                    for (const auto& point : result.points) {
                        if (!includedInChart(request, point)) {
                            continue;
                        }
                        const auto categoryKey = category(point.sector);
                        requireKnownCategory(knownCategories, categoryKey);
                        const auto person = dimensionValue(point.person);
                        observedCategories.insert(categoryKey);
                        checkedAdd(counts[person][categoryKey], point.count);
                        checkedAdd(totals[person], point.count);
                        checkedAdd(total, point.count);
                    }

                    const bool known = totalIsKnown(request, result);
                    for (const auto& [person, personCounts] : counts) {
                        model.series.push_back(makeSeries(
                            person,
                            valuesFor(request, model.categories, personCounts, observedCategories),
                            optionalTotal(known, totals.at(person))));
                    }
                    model.total = optionalTotal(known, total);
                    return model;
                }
                return buildSingleSeries(request, result, false);
            }

            AnalyticsChartModel model;
            model.categories = temporalCategories(request);
            const std::set<std::string> knownCategories(model.categories.begin(),
                                                        model.categories.end());
            std::map<std::string, CountByCategory> counts;
            std::set<std::string> observedCategories;
            addObservedBuckets(request, result, knownCategories, observedCategories);
            std::map<std::string, std::int64_t> totals;
            std::int64_t total = 0;
            for (const auto& seriesName : selectedDimensionKeys(request)) {
                counts.try_emplace(seriesName);
                totals.try_emplace(seriesName, 0);
            }
            for (const auto& point : result.points) {
                if (!includedInChart(request, point)) {
                    continue;
                }
                requireKnownCategory(knownCategories, point.bucketKey);
                const auto seriesName = dimensionKey(request, point);
                observedCategories.insert(point.bucketKey);
                checkedAdd(counts[seriesName][point.bucketKey], point.count);
                checkedAdd(totals[seriesName], point.count);
                checkedAdd(total, point.count);
            }

            const bool known = totalIsKnown(request, result);
            for (const auto& [seriesName, seriesCounts] : counts) {
                AnalyticsChartSeries series = makeSeries(
                    seriesName,
                    valuesFor(request, model.categories, seriesCounts, observedCategories),
                    optionalTotal(known, totals.at(seriesName)));
                if (request.grain == domain::TimeGrain::IsoReferenceMonth) {
                    series.trendValues = trendFor(series.values);
                }
                model.series.push_back(std::move(series));
            }
            model.total = optionalTotal(known, total);
            return model;
        }

        domain::AnalyticsRequest dashboardRequest(const domain::AnalyticsMetric metric,
                                                  const domain::AnalyticsPeriod& period,
                                                  const domain::TimeGrain grain,
                                                  const domain::Breakdown breakdown) {
            return {.metric = metric,
                    .period = period,
                    .grain = grain,
                    .breakdown = breakdown,
                    .personRole = domain::PersonRole::Executor};
        }

    } // namespace

    AnalyticsChartModel ActivityAnalyticsChartModelBuilder::build(
        const domain::AnalyticsRequest& request, const domain::AnalyticsSeriesResult& result,
        const AnalyticsChartMode mode, const AnalyticsChartScale scale) {
        validateChartRequest(request);
        if (scale == AnalyticsChartScale::Percentage &&
            mode != AnalyticsChartMode::DeadlineStacked) {
            throw std::invalid_argument("percentage scale requires a deadline stack");
        }
        if (mode == AnalyticsChartMode::DeadlineStacked &&
            request.metric != domain::AnalyticsMetric::PendingDeadline) {
            throw std::invalid_argument("deadline stack requires deadline analytics");
        }

        AnalyticsChartModel model;
        model.percentage = scale == AnalyticsChartScale::Percentage;
        model.subtitle = observedSubtitle(result);
        if (!result.available()) {
            model.qualityNote = result.unavailableReason.empty() ? "analytics_result_incomplete"
                                                                 : result.unavailableReason;
            return model;
        }

        const auto excluded = validateDeadlineQuality(request, result);
        switch (mode) {
        case AnalyticsChartMode::SimpleBar:
            model = buildSingleSeries(request, result, false);
            break;
        case AnalyticsChartMode::CohortStacked:
            model = buildCohortStack(request, result);
            break;
        case AnalyticsChartMode::DeadlineStacked:
            model = buildDeadlineStack(request, result);
            if (scale == AnalyticsChartScale::Percentage) {
                normalizeDeadlinePercentages(model);
            }
            break;
        case AnalyticsChartMode::LineTotal:
            model = buildSingleSeries(request, result, true);
            break;
        case AnalyticsChartMode::Custom:
            model = buildCustom(request, result);
            break;
        }

        model.percentage = scale == AnalyticsChartScale::Percentage;
        model.subtitle = observedSubtitle(result);
        if (excluded != 0) {
            model.qualityNote = "excluded_for_data_quality=" + std::to_string(excluded);
        }
        if (const auto stale = staleSnapshotQuality(request, result); !stale.empty()) {
            if (!model.qualityNote.empty()) {
                model.qualityNote += " | ";
            }
            model.qualityNote += stale;
        }
        return model;
    }

    ActivityAnalyticsDashboardCharts ActivityAnalyticsChartModelBuilder::buildDashboard(
        const domain::AnalyticsPeriod& reportPeriod, const domain::AnalyticsPeriod& historyPeriod,
        const ActivityAnalyticsDashboard& dashboard) {
        using domain::AnalyticsMetric;
        using domain::Breakdown;
        using domain::TimeGrain;

        const auto bySector = Breakdown::DivisionSector;
        const auto byDivision = Breakdown::Division;
        return {
            .registeredBySector =
                build(dashboardRequest(AnalyticsMetric::Registered, reportPeriod,
                                       TimeGrain::WholePeriod, bySector),
                      dashboard.registeredBySector, AnalyticsChartMode::SimpleBar),
            .registeredMonthly = build(dashboardRequest(AnalyticsMetric::Registered, historyPeriod,
                                                        TimeGrain::IsoReferenceMonth, byDivision),
                                       dashboard.registeredMonthly, AnalyticsChartMode::SimpleBar),
            .executedBySector =
                build(dashboardRequest(AnalyticsMetric::Executed, reportPeriod,
                                       TimeGrain::WholePeriod, bySector),
                      dashboard.executedBySector, AnalyticsChartMode::CohortStacked),
            .executedMonthly = build(dashboardRequest(AnalyticsMetric::Executed, historyPeriod,
                                                      TimeGrain::IsoReferenceMonth, byDivision),
                                     dashboard.executedMonthly, AnalyticsChartMode::SimpleBar),
            .partialAttentionBySector =
                build(dashboardRequest(AnalyticsMetric::PartialAttention, reportPeriod,
                                       TimeGrain::WholePeriod, bySector),
                      dashboard.partialAttentionBySector, AnalyticsChartMode::CohortStacked),
            .spgBySector = build(dashboardRequest(AnalyticsMetric::Spg, reportPeriod,
                                                  TimeGrain::WholePeriod, bySector),
                                 dashboard.spgBySector, AnalyticsChartMode::CohortStacked),
            .apgBySector = build(dashboardRequest(AnalyticsMetric::Apg, reportPeriod,
                                                  TimeGrain::WholePeriod, bySector),
                                 dashboard.apgBySector, AnalyticsChartMode::CohortStacked),
            .aplBySector = build(dashboardRequest(AnalyticsMetric::Apl, reportPeriod,
                                                  TimeGrain::WholePeriod, bySector),
                                 dashboard.aplBySector, AnalyticsChartMode::CohortStacked),
            .pendingBySector = build(dashboardRequest(AnalyticsMetric::Pending, reportPeriod,
                                                      TimeGrain::WholePeriod, bySector),
                                     dashboard.pendingBySector, AnalyticsChartMode::SimpleBar),
            .pendingMonthly = build(dashboardRequest(AnalyticsMetric::Pending, historyPeriod,
                                                     TimeGrain::IsoReferenceMonth, byDivision),
                                    dashboard.pendingMonthly, AnalyticsChartMode::SimpleBar),
            .issuedByDivision = build(dashboardRequest(AnalyticsMetric::Issued, reportPeriod,
                                                       TimeGrain::WholePeriod, byDivision),
                                      dashboard.issuedByDivision, AnalyticsChartMode::SimpleBar),
            .issuedMonthly = build(dashboardRequest(AnalyticsMetric::Issued, historyPeriod,
                                                    TimeGrain::IsoReferenceMonth, byDivision),
                                   dashboard.issuedMonthly, AnalyticsChartMode::SimpleBar),
            .pendingDeadlinePercentage =
                build(dashboardRequest(AnalyticsMetric::PendingDeadline, reportPeriod,
                                       TimeGrain::IsoWeek, bySector),
                      dashboard.pendingDeadlineWeekly, AnalyticsChartMode::DeadlineStacked,
                      AnalyticsChartScale::Percentage),
            .pendingDeadlineQuantity =
                build(dashboardRequest(AnalyticsMetric::PendingDeadline, reportPeriod,
                                       TimeGrain::IsoWeek, bySector),
                      dashboard.pendingDeadlineWeekly, AnalyticsChartMode::DeadlineStacked),
        };
    }

} // namespace ssa::application
