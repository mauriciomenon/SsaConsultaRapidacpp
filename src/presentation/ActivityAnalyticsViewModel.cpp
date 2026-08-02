#include "presentation/ActivityAnalyticsViewModel.h"

#include "application/ActivityAnalyticsChartModelBuilder.h"

#include <QDate>
#include <QDebug>
#include <QFile>
#include <QMetaObject>
#include <QMetaType>
#include <QStringList>
#include <QThreadPool>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ssa::presentation {

    namespace {

        QVariant optionalInt(const std::optional<int>& value) {
            return value ? QVariant{*value} : QVariant{};
        }

        QVariant optionalDouble(const std::optional<double>& value) {
            return value ? QVariant{*value} : QVariant{};
        }

        QVariantMap calendarMonthMap(const int year, const int month) {
            const auto period = domain::calendarMonthPeriod(year, month);
            return {{QStringLiteral("year"), year},
                    {QStringLiteral("month"), month},
                    {QStringLiteral("firstYear"), period.first.year},
                    {QStringLiteral("firstWeek"), period.first.week},
                    {QStringLiteral("lastYear"), period.last.year},
                    {QStringLiteral("lastWeek"), period.last.week}};
        }

        QVariantMap isoReferenceMonthMap(const int year, const int month) {
            const auto period = domain::isoReferenceMonthPeriod(year, month);
            return {{QStringLiteral("year"), year},
                    {QStringLiteral("month"), month},
                    {QStringLiteral("firstYear"), period.first.year},
                    {QStringLiteral("firstWeek"), period.first.week},
                    {QStringLiteral("lastYear"), period.last.year},
                    {QStringLiteral("lastWeek"), period.last.week}};
        }

        std::pair<int, int> previousMonth(const int year, const int month) {
            if (month == 1) {
                return {year - 1, 12};
            }
            return {year, month - 1};
        }

        QVariantMap seriesMetadataMap(const domain::AnalyticsSeriesResult& series,
                                      const domain::AnalyticsMetric metric) {
            return {
                {QStringLiteral("metric"), static_cast<int>(metric)},
                {QStringLiteral("sourceRevision"), QString::fromStdString(series.sourceRevision)},
                {QStringLiteral("observedIsoYearWeek"), optionalInt(series.observedIsoYearWeek)},
                {QStringLiteral("excludedForDataQuality"),
                 QVariant::fromValue<qlonglong>(series.excludedForDataQuality)},
                {QStringLiteral("complete"), series.complete},
                {QStringLiteral("available"), series.available()},
                {QStringLiteral("unavailableReason"),
                 QString::fromStdString(series.unavailableReason)}};
        }

        QStringList stringList(const std::vector<std::string>& values) {
            QStringList result;
            result.reserve(static_cast<qsizetype>(values.size()));
            std::ranges::transform(values, std::back_inserter(result),
                                   [](const auto& value) { return QString::fromStdString(value); });
            return result;
        }

        QStringList displayCategories(const std::vector<std::string>& categories) {
            QStringList result;
            result.reserve(static_cast<qsizetype>(categories.size()));
            std::ranges::transform(
                categories, std::back_inserter(result), [](const std::string& category) {
                    return QString::fromStdString(domain::formatAnalyticsBucketLabel(category));
                });
            return result;
        }

        QVariantList optionalValues(const std::vector<std::optional<double>>& values) {
            QVariantList result;
            result.reserve(static_cast<qsizetype>(values.size()));
            std::ranges::transform(values, std::back_inserter(result), optionalDouble);
            return result;
        }

        QVariantMap chartReadySeriesMap(const domain::AnalyticsSeriesResult& result,
                                        const domain::AnalyticsMetric metric,
                                        const application::AnalyticsChartModel& chart) {
            auto model = seriesMetadataMap(result, metric);
            model.insert(QStringLiteral("categories"), displayCategories(chart.categories));
            QVariantList series;
            series.reserve(static_cast<qsizetype>(chart.series.size()));
            QVariantList allTrends;
            allTrends.reserve(static_cast<qsizetype>(chart.series.size()));
            for (const auto& chartSeries : chart.series) {
                const auto trends = optionalValues(chartSeries.trendValues);
                series.push_back(QVariantMap{
                    {QStringLiteral("name"), QString::fromStdString(chartSeries.name)},
                    {QStringLiteral("tag"), QString::fromStdString(chartSeries.tag)},
                    {QStringLiteral("values"), optionalValues(chartSeries.values)},
                    {QStringLiteral("trendValues"), trends},
                    {QStringLiteral("total"), optionalDouble(chartSeries.total)},
                });
                allTrends.push_back(trends);
            }
            model.insert(QStringLiteral("series"), series);
            model.insert(QStringLiteral("trendValues"),
                         chart.series.size() == 1 ? allTrends.value(0) : QVariant{allTrends});
            model.insert(QStringLiteral("total"), optionalDouble(chart.total));
            model.insert(QStringLiteral("subtitle"), QString::fromStdString(chart.subtitle));
            model.insert(QStringLiteral("qualityText"), QString::fromStdString(chart.qualityNote));
            model.insert(QStringLiteral("percentage"), chart.percentage);
            return model;
        }

        QVariantMap dimensionMap(const domain::AnalyticsDimensionValues& values) {
            return {{QStringLiteral("divisions"), stringList(values.divisions)},
                    {QStringLiteral("sectors"), stringList(values.sectors)},
                    {QStringLiteral("people"), stringList(values.people)}};
        }

        QVariantList
        availabilityList(const std::vector<domain::AnalyticsMetricAvailability>& values) {
            QVariantList result;
            result.reserve(static_cast<qsizetype>(values.size()));
            std::ranges::transform(
                values, std::back_inserter(result),
                [](const domain::AnalyticsMetricAvailability& value) -> QVariant {
                    return QVariantMap{
                        {QStringLiteral("metric"), static_cast<int>(value.metric)},
                        {QStringLiteral("firstIsoYearWeek"), optionalInt(value.firstIsoYearWeek)},
                        {QStringLiteral("lastIsoYearWeek"), optionalInt(value.lastIsoYearWeek)},
                        {QStringLiteral("available"), value.available},
                        {QStringLiteral("reason"), QString::fromStdString(value.reason)}};
                });
            return result;
        }

        QVariantMap dashboardMap(const application::ActivityAnalyticsDashboard& dashboard,
                                 const application::ActivityAnalyticsDashboardCharts& charts) {
            return {
                {QStringLiteral("registeredBySector"),
                 chartReadySeriesMap(dashboard.registeredBySector,
                                     domain::AnalyticsMetric::Registered,
                                     charts.registeredBySector)},
                {QStringLiteral("registeredMonthly"),
                 chartReadySeriesMap(dashboard.registeredMonthly,
                                     domain::AnalyticsMetric::Registered,
                                     charts.registeredMonthly)},
                {QStringLiteral("executedBySector"),
                 chartReadySeriesMap(dashboard.executedBySector, domain::AnalyticsMetric::Executed,
                                     charts.executedBySector)},
                {QStringLiteral("executedMonthly"),
                 chartReadySeriesMap(dashboard.executedMonthly, domain::AnalyticsMetric::Executed,
                                     charts.executedMonthly)},
                {QStringLiteral("partialAttentionBySector"),
                 chartReadySeriesMap(dashboard.partialAttentionBySector,
                                     domain::AnalyticsMetric::PartialAttention,
                                     charts.partialAttentionBySector)},
                {QStringLiteral("spgBySector"),
                 chartReadySeriesMap(dashboard.spgBySector, domain::AnalyticsMetric::Spg,
                                     charts.spgBySector)},
                {QStringLiteral("apgBySector"),
                 chartReadySeriesMap(dashboard.apgBySector, domain::AnalyticsMetric::Apg,
                                     charts.apgBySector)},
                {QStringLiteral("aplBySector"),
                 chartReadySeriesMap(dashboard.aplBySector, domain::AnalyticsMetric::Apl,
                                     charts.aplBySector)},
                {QStringLiteral("pendingBySector"),
                 chartReadySeriesMap(dashboard.pendingBySector, domain::AnalyticsMetric::Pending,
                                     charts.pendingBySector)},
                {QStringLiteral("pendingMonthly"),
                 chartReadySeriesMap(dashboard.pendingMonthly, domain::AnalyticsMetric::Pending,
                                     charts.pendingMonthly)},
                {QStringLiteral("issuedByDivision"),
                 chartReadySeriesMap(dashboard.issuedByDivision, domain::AnalyticsMetric::Issued,
                                     charts.issuedByDivision)},
                {QStringLiteral("issuedMonthly"),
                 chartReadySeriesMap(dashboard.issuedMonthly, domain::AnalyticsMetric::Issued,
                                     charts.issuedMonthly)},
                {QStringLiteral("pendingDeadlinePercentage"),
                 chartReadySeriesMap(dashboard.pendingDeadlineWeekly,
                                     domain::AnalyticsMetric::PendingDeadline,
                                     charts.pendingDeadlinePercentage)},
                {QStringLiteral("pendingDeadlineQuantity"),
                 chartReadySeriesMap(dashboard.pendingDeadlineWeekly,
                                     domain::AnalyticsMetric::PendingDeadline,
                                     charts.pendingDeadlineQuantity)},
            };
        }

        int selectionInt(const QVariantMap& selection, const QString& key) {
            const auto found = selection.constFind(key);
            if (found == selection.cend()) {
                throw std::invalid_argument("missing field: " + key.toStdString());
            }
            bool numeric = false;
            const double raw = found->toDouble(&numeric);
            if (!numeric || !std::isfinite(raw) || std::trunc(raw) != raw ||
                raw < static_cast<double>((std::numeric_limits<int>::min)()) ||
                raw > static_cast<double>((std::numeric_limits<int>::max)())) {
                throw std::invalid_argument("invalid integer field: " + key.toStdString());
            }
            return static_cast<int>(raw);
        }

        std::optional<int> warningWindow(const QVariantMap& selection) {
            const auto key = QStringLiteral("warningWindowDays");
            const auto found = selection.constFind(key);
            if (found == selection.cend() || found->isNull()) {
                return std::nullopt;
            }
            const int value = selectionInt(selection, key);
            if (value < 0 || value > 365) {
                throw std::invalid_argument("warning window must be between 0 and 365 days");
            }
            return value;
        }

        domain::AnalyticsPeriod selectionPeriod(const QVariantMap& selection,
                                                const QString& prefix = {}) {
            const auto firstYearKey = prefix.isEmpty() ? QStringLiteral("firstYear")
                                                       : prefix + QStringLiteral("FirstYear");
            const auto firstWeekKey = prefix.isEmpty() ? QStringLiteral("firstWeek")
                                                       : prefix + QStringLiteral("FirstWeek");
            const auto lastYearKey =
                prefix.isEmpty() ? QStringLiteral("lastYear") : prefix + QStringLiteral("LastYear");
            const auto lastWeekKey =
                prefix.isEmpty() ? QStringLiteral("lastWeek") : prefix + QStringLiteral("LastWeek");
            domain::AnalyticsPeriod period{.first = {selectionInt(selection, firstYearKey),
                                                     selectionInt(selection, firstWeekKey)},
                                           .last = {selectionInt(selection, lastYearKey),
                                                    selectionInt(selection, lastWeekKey)}};
            if (!domain::isValidPeriod(period)) {
                throw std::invalid_argument("analytics period is invalid");
            }
            return period;
        }

        domain::AnalyticsMetric selectionMetric(const int value) {
            switch (value) {
            case 0:
                return domain::AnalyticsMetric::Registered;
            case 1:
                return domain::AnalyticsMetric::Executed;
            case 2:
                return domain::AnalyticsMetric::PartialAttention;
            case 3:
                return domain::AnalyticsMetric::Spg;
            case 4:
                return domain::AnalyticsMetric::Apg;
            case 5:
                return domain::AnalyticsMetric::Apl;
            case 6:
                return domain::AnalyticsMetric::Pending;
            case 7:
                return domain::AnalyticsMetric::Issued;
            case 8:
                return domain::AnalyticsMetric::PendingDeadline;
            default:
                throw std::invalid_argument("analytics metric is invalid");
            }
        }

        domain::TimeGrain selectionGrain(const int value) {
            switch (value) {
            case 0:
                return domain::TimeGrain::WholePeriod;
            case 1:
                return domain::TimeGrain::IsoWeek;
            case 2:
                return domain::TimeGrain::IsoReferenceMonth;
            default:
                throw std::invalid_argument("analytics grain is invalid");
            }
        }

        domain::Breakdown selectionBreakdown(const int value) {
            switch (value) {
            case 0:
                return domain::Breakdown::Division;
            case 1:
                return domain::Breakdown::DivisionSector;
            case 2:
                return domain::Breakdown::DivisionPerson;
            case 3:
                return domain::Breakdown::DivisionSectorPerson;
            default:
                throw std::invalid_argument("analytics breakdown is invalid");
            }
        }

        domain::PersonRole selectionPersonRole(const int value) {
            switch (value) {
            case 0:
                return domain::PersonRole::Requester;
            case 1:
                return domain::PersonRole::Planner;
            case 2:
                return domain::PersonRole::Executor;
            default:
                throw std::invalid_argument("analytics person role is invalid");
            }
        }

        std::vector<std::string> selectionStrings(const QVariantMap& selection,
                                                  const QString& key) {
            const auto found = selection.constFind(key);
            if (found == selection.cend() || found->isNull()) {
                return {};
            }
            QStringList strings;
            if (found->metaType().id() == QMetaType::QStringList) {
                strings = found->toStringList();
            } else if (found->metaType().id() == QMetaType::QVariantList) {
                const auto values = found->toList();
                strings.reserve(values.size());
                for (const auto& value : values) {
                    if (value.metaType().id() != QMetaType::QString) {
                        throw std::invalid_argument("invalid string list field: " +
                                                    key.toStdString());
                    }
                    strings.push_back(value.toString());
                }
            } else {
                throw std::invalid_argument("invalid string list field: " + key.toStdString());
            }
            std::vector<std::string> result;
            result.reserve(static_cast<std::size_t>(strings.size()));
            std::ranges::transform(strings, std::back_inserter(result),
                                   [](const auto& value) { return value.toStdString(); });
            return result;
        }

        domain::AnalyticsRequest analyticsRequest(const QVariantMap& selection) {
            return {.metric = selectionMetric(selectionInt(selection, QStringLiteral("metric"))),
                    .period = selectionPeriod(selection),
                    .grain = selectionGrain(selectionInt(selection, QStringLiteral("grain"))),
                    .breakdown =
                        selectionBreakdown(selectionInt(selection, QStringLiteral("breakdown"))),
                    .personRole =
                        selectionPersonRole(selectionInt(selection, QStringLiteral("personRole"))),
                    .divisions = selectionStrings(selection, QStringLiteral("divisions")),
                    .sectors = selectionStrings(selection, QStringLiteral("sectors")),
                    .people = selectionStrings(selection, QStringLiteral("people")),
                    .warningWindowDays = warningWindow(selection)};
        }

        QString exceptionMessage(const std::exception_ptr& error) {
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& exception) {
                const auto message = QString::fromUtf8(exception.what()).trimmed();
                return message.isEmpty() ? QStringLiteral("Falha ao carregar analises") : message;
            } catch (...) {
                return QStringLiteral("Falha interna ao carregar analises");
            }
        }

        QString formatIsoWeekLabel(const int year, const int week) {
            return QString::fromStdString(domain::formatIsoYearWeekDisplay(year, week));
        }

        QString metricTitleLabel(const domain::AnalyticsMetric metric) {
            switch (metric) {
            case domain::AnalyticsMetric::Registered:
                return QStringLiteral("SSAs cadastradas");
            case domain::AnalyticsMetric::Executed:
                return QStringLiteral("SSAs executadas");
            case domain::AnalyticsMetric::PartialAttention:
                return QStringLiteral("SSAs com atencao parcial");
            case domain::AnalyticsMetric::Spg:
                return QStringLiteral("SSAs SPG");
            case domain::AnalyticsMetric::Apg:
                return QStringLiteral("SSAs APG");
            case domain::AnalyticsMetric::Apl:
                return QStringLiteral("SSAs APL");
            case domain::AnalyticsMetric::Pending:
                return QStringLiteral("SSAs pendentes");
            case domain::AnalyticsMetric::Issued:
                return QStringLiteral("SSAs emitidas");
            case domain::AnalyticsMetric::PendingDeadline:
                return QStringLiteral("Prazo das SSAs pendentes");
            }
            return QStringLiteral("SSAs");
        }

        QString breakdownTitleLabel(const domain::Breakdown breakdown) {
            switch (breakdown) {
            case domain::Breakdown::Division:
                return QStringLiteral("Divisao");
            case domain::Breakdown::DivisionSector:
                return QStringLiteral("Divisao e setor");
            case domain::Breakdown::DivisionPerson:
                return QStringLiteral("Divisao e pessoa");
            case domain::Breakdown::DivisionSectorPerson:
                return QStringLiteral("Setor e pessoa");
            }
            return QStringLiteral("Categoria");
        }

        QString personRoleTitleLabel(const domain::PersonRole role) {
            switch (role) {
            case domain::PersonRole::Requester:
                return QStringLiteral("Solicitante");
            case domain::PersonRole::Planner:
                return QStringLiteral("Planejamento/programacao");
            case domain::PersonRole::Executor:
                return QStringLiteral("Execucao");
            }
            return QStringLiteral("Pessoa");
        }

        QString customChartTitleFromSelection(const QVariantMap& selection) {
            const auto request = analyticsRequest(selection);
            const auto firstWeek =
                formatIsoWeekLabel(request.period.first.year, request.period.first.week);
            const auto lastWeek =
                formatIsoWeekLabel(request.period.last.year, request.period.last.week);
            const auto periodText = firstWeek == lastWeek
                                        ? firstWeek
                                        : QStringLiteral("de %1 a %2").arg(firstWeek, lastWeek);
            return QStringLiteral("%1 %2 em %3 (%4)")
                .arg(metricTitleLabel(request.metric), periodText,
                     breakdownTitleLabel(request.breakdown),
                     personRoleTitleLabel(request.personRole));
        }

        std::string sectorSortKey(const std::string& category, const domain::Breakdown breakdown) {
            if (breakdown == domain::Breakdown::DivisionSector ||
                breakdown == domain::Breakdown::DivisionPerson) {
                if (const auto separator = category.rfind(" / "); separator != std::string::npos) {
                    return category.substr(separator + 3);
                }
            }
            if (breakdown == domain::Breakdown::DivisionSectorPerson) {
                if (const auto separator = category.find('\n'); separator != std::string::npos) {
                    return category.substr(separator + 1);
                }
            }
            return category;
        }

        void sortChartModelCategories(application::AnalyticsChartModel& chart,
                                      const domain::Breakdown breakdown, const int categorySort) {
            if (categorySort != 1 || chart.categories.size() < 2) {
                return;
            }
            std::vector<std::size_t> order(chart.categories.size());
            std::iota(order.begin(), order.end(), 0);
            std::ranges::sort(order, [&](const std::size_t left, const std::size_t right) {
                const auto leftKey = sectorSortKey(chart.categories[left], breakdown);
                const auto rightKey = sectorSortKey(chart.categories[right], breakdown);
                if (leftKey != rightKey) {
                    return leftKey < rightKey;
                }
                return chart.categories[left] < chart.categories[right];
            });
            const auto reorderValues = [&order](std::vector<std::optional<double>>& values) {
                if (values.size() != order.size()) {
                    return;
                }
                std::vector<std::optional<double>> sorted;
                sorted.reserve(values.size());
                for (const auto index : order) {
                    sorted.push_back(values[index]);
                }
                values = std::move(sorted);
            };
            std::vector<std::string> sortedCategories;
            sortedCategories.reserve(chart.categories.size());
            for (const auto index : order) {
                sortedCategories.push_back(chart.categories[index]);
            }
            chart.categories = std::move(sortedCategories);
            for (auto& series : chart.series) {
                reorderValues(series.values);
                reorderValues(series.trendValues);
            }
        }

        int selectionCategorySort(const QVariantMap& selection) {
            const auto found = selection.constFind(QStringLiteral("categorySort"));
            if (found == selection.cend() || found->isNull()) {
                return 0;
            }
            bool numeric = false;
            const int value = found->toInt(&numeric);
            if (!numeric || value < 0 || value > 1) {
                throw std::invalid_argument("category sort is invalid");
            }
            return value;
        }

    } // namespace

    ActivityAnalyticsViewModel::ActivityAnalyticsViewModel(
        std::shared_ptr<const application::ActivityAnalyticsService> service, QObject* parent)
        : QObject(parent), service_(std::move(service)) {
        if (!service_) {
            throw std::invalid_argument("activity analytics service is required");
        }
    }

    ActivityAnalyticsViewModel::~ActivityAnalyticsViewModel() {
        shuttingDown_ = true;
        ++generation_;
        for (const auto& operation : operations_) {
            disconnect(&operation->watcher, nullptr, this, nullptr);
            stop(*operation);
        }
        operations_.clear();
    }

    bool ActivityAnalyticsViewModel::loading() const noexcept {
        return loading_;
    }

    bool ActivityAnalyticsViewModel::canceling() const noexcept {
        return canceling_;
    }

    bool ActivityAnalyticsViewModel::canCancel() const noexcept {
        return loading_ && !canceling_ && hasActiveOperations();
    }

    QString ActivityAnalyticsViewModel::errorMessage() const {
        return errorMessage_;
    }

    const QVariantMap& ActivityAnalyticsViewModel::dashboard() const noexcept {
        return dashboard_;
    }

    const QVariantMap& ActivityAnalyticsViewModel::customSeries() const noexcept {
        return customSeries_;
    }

    const QVariantMap& ActivityAnalyticsViewModel::dimensionValues() const noexcept {
        return dimensionValues_;
    }

    const QVariantList& ActivityAnalyticsViewModel::availability() const noexcept {
        return availability_;
    }

    const QVariant& ActivityAnalyticsViewModel::warningWindowDays() const noexcept {
        return warningWindowDays_;
    }

    bool ActivityAnalyticsViewModel::hasActiveOperations() const noexcept {
        return std::ranges::any_of(operations_,
                                   [](const auto& operation) { return !operation->completed; });
    }

    void ActivityAnalyticsViewModel::loadDashboard(const domain::AnalyticsPeriod& reportPeriod,
                                                   const domain::AnalyticsPeriod& historyPeriod,
                                                   const std::optional<int> warningWindowDays) {
        const auto service = service_;
        start(
            RequestKind::Dashboard,
            [service, reportPeriod, historyPeriod,
             warningWindowDays](const std::stop_token& stopToken) -> TaskResult {
                const auto dashboard = service->loadDashboard(reportPeriod, historyPeriod,
                                                              warningWindowDays, stopToken);
                const auto charts = application::ActivityAnalyticsChartModelBuilder::buildDashboard(
                    reportPeriod, historyPeriod, dashboard);
                DashboardPayload payload{.model = dashboardMap(dashboard, charts)};
                const auto appendUnavailable =
                    [&payload](const domain::AnalyticsMetric metric,
                               const domain::AnalyticsSeriesResult& series) {
                        if (!series.available()) {
                            payload.unavailableMetrics.push_back(
                                {.metric = metric,
                                 .reason = QString::fromStdString(series.unavailableReason)});
                        }
                    };
                appendUnavailable(domain::AnalyticsMetric::Registered,
                                  dashboard.registeredBySector);
                appendUnavailable(domain::AnalyticsMetric::Registered, dashboard.registeredMonthly);
                appendUnavailable(domain::AnalyticsMetric::Executed, dashboard.executedBySector);
                appendUnavailable(domain::AnalyticsMetric::Executed, dashboard.executedMonthly);
                appendUnavailable(domain::AnalyticsMetric::PartialAttention,
                                  dashboard.partialAttentionBySector);
                appendUnavailable(domain::AnalyticsMetric::Spg, dashboard.spgBySector);
                appendUnavailable(domain::AnalyticsMetric::Apg, dashboard.apgBySector);
                appendUnavailable(domain::AnalyticsMetric::Apl, dashboard.aplBySector);
                appendUnavailable(domain::AnalyticsMetric::Pending, dashboard.pendingBySector);
                appendUnavailable(domain::AnalyticsMetric::Pending, dashboard.pendingMonthly);
                appendUnavailable(domain::AnalyticsMetric::Issued, dashboard.issuedByDivision);
                appendUnavailable(domain::AnalyticsMetric::Issued, dashboard.issuedMonthly);
                appendUnavailable(domain::AnalyticsMetric::PendingDeadline,
                                  dashboard.pendingDeadlineWeekly);
                return payload;
            });
    }

    void ActivityAnalyticsViewModel::loadCustomSeries(domain::AnalyticsRequest request,
                                                      const int categorySort) {
        const auto service = service_;
        const auto breakdown = request.breakdown;
        start(RequestKind::CustomSeries,
              [service, request = std::move(request), categorySort,
               breakdown](const std::stop_token& stopToken) -> TaskResult {
                  auto result = service->series(request, stopToken);
                  const auto mode = request.metric == domain::AnalyticsMetric::PendingDeadline
                                        ? application::AnalyticsChartMode::DeadlineStacked
                                        : application::AnalyticsChartMode::Custom;
                  auto chart =
                      application::ActivityAnalyticsChartModelBuilder::build(request, result, mode);
                  sortChartModelCategories(chart, breakdown, categorySort);
                  return CustomSeriesPayload{
                      .model = chartReadySeriesMap(result, request.metric, chart),
                      .metric = request.metric,
                      .unavailable = !result.available(),
                      .unavailableReason = QString::fromStdString(result.unavailableReason),
                  };
              });
    }

    void ActivityAnalyticsViewModel::loadDimensionValues(domain::AnalyticsRequest request) {
        const auto service = service_;
        start(RequestKind::DimensionValues,
              [service, request = std::move(request)](const std::stop_token& stopToken)
                  -> TaskResult { return service->dimensionValues(request, stopToken); });
    }

    void ActivityAnalyticsViewModel::loadAvailability() {
        const auto service = service_;
        start(RequestKind::Availability, [service](const std::stop_token& stopToken) -> TaskResult {
            return service->availability(stopToken);
        });
    }

    void ActivityAnalyticsViewModel::loadWarningWindowDays() {
        const auto service = service_;
        start(RequestKind::WarningWindowLoad,
              [service](const std::stop_token& stopToken) -> TaskResult {
                  return WarningWindowPayload{.days = service->warningWindowDays(stopToken)};
              });
    }

    bool ActivityAnalyticsViewModel::saveWarningWindowDays(const int days) {
        if (days < 0 || days > 365) {
            const std::invalid_argument error("warning window must be between 0 and 365 days");
            rejectSelection(error);
            return false;
        }
        const auto service = service_;
        start(RequestKind::WarningWindowSave,
              [service, days](const std::stop_token& stopToken) -> TaskResult {
                  service->setWarningWindowDays(days, stopToken);
                  return WarningWindowPayload{.days = days};
              });
        return true;
    }

    QVariantMap ActivityAnalyticsViewModel::currentMonthSelection() const {
        const QDate month = QDate::currentDate().addMonths(-1);
        return calendarMonthMap(month.year(), month.month());
    }

    QVariantMap ActivityAnalyticsViewModel::calendarMonthSelection(const int year,
                                                                   const int month) const {
        return calendarMonthMap(year, month);
    }

    QVariantMap ActivityAnalyticsViewModel::yearToDateSelection() const {
        const auto selection = domain::yearToDateCalendarSelection(
            QDate::currentDate().toString(Qt::ISODate).toStdString());
        if (!selection.has_value()) {
            return currentMonthSelection();
        }
        return {{QStringLiteral("year"), selection->first.year},
                {QStringLiteral("month"), 0},
                {QStringLiteral("firstYear"), selection->first.year},
                {QStringLiteral("firstWeek"), selection->first.week},
                {QStringLiteral("lastYear"), selection->last.year},
                {QStringLiteral("lastWeek"), selection->last.week}};
    }

    QVariantMap ActivityAnalyticsViewModel::currentIsoWeekSelection() const {
        const auto currentWeek =
            domain::isoWeekForDate(QDate::currentDate().toString(Qt::ISODate).toStdString());
        if (!currentWeek.has_value()) {
            return currentMonthSelection();
        }
        const auto monthParts = domain::isoReferenceMonthParts(*currentWeek);
        if (!monthParts.has_value()) {
            return currentMonthSelection();
        }
        return {{QStringLiteral("year"), monthParts->year},
                {QStringLiteral("month"), monthParts->month},
                {QStringLiteral("firstYear"), currentWeek->year},
                {QStringLiteral("firstWeek"), currentWeek->week},
                {QStringLiteral("lastYear"), currentWeek->year},
                {QStringLiteral("lastWeek"), currentWeek->week}};
    }

    QVariantMap ActivityAnalyticsViewModel::currentIsoMonthSelection() const {
        const auto currentWeek =
            domain::isoWeekForDate(QDate::currentDate().toString(Qt::ISODate).toStdString());
        if (!currentWeek.has_value()) {
            return currentMonthSelection();
        }
        const auto monthParts = domain::isoReferenceMonthParts(*currentWeek);
        if (!monthParts.has_value()) {
            return currentMonthSelection();
        }
        const auto previous = previousMonth(monthParts->year, monthParts->month);
        return isoReferenceMonthMap(previous.first, previous.second);
    }

    void ActivityAnalyticsViewModel::clearCustomSeries() {
        if (customSeries_.isEmpty()) {
            return;
        }
        customSeries_.clear();
        emit customSeriesChanged();
    }

    QString ActivityAnalyticsViewModel::customChartTitle(const QVariantMap& selection) const {
        try {
            return customChartTitleFromSelection(selection);
        } catch (const std::exception&) {
            return QStringLiteral("Resultado da analise customizada");
        }
    }

    bool ActivityAnalyticsViewModel::writeExportFile(const QString& path,
                                                     const QString& content) const {
        if (path.trimmed().isEmpty()) {
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return false;
        }
        return file.write(content.toUtf8()) >= 0;
    }

    bool ActivityAnalyticsViewModel::requestDashboard(const QVariantMap& selection) {
        try {
            const auto reportPeriod = selectionPeriod(selection, QStringLiteral("report"));
            loadDashboard(reportPeriod, domain::referenceMonthHistoryPeriod(reportPeriod.last, 13),
                          warningWindow(selection));
            return true;
        } catch (const std::exception& error) {
            rejectSelection(error);
            return false;
        }
    }

    bool ActivityAnalyticsViewModel::requestCustomSeries(const QVariantMap& selection) {
        try {
            loadCustomSeries(analyticsRequest(selection), selectionCategorySort(selection));
            return true;
        } catch (const std::exception& error) {
            rejectSelection(error);
            return false;
        }
    }

    bool ActivityAnalyticsViewModel::requestDimensionValues(const QVariantMap& selection) {
        try {
            loadDimensionValues(analyticsRequest(selection));
            return true;
        } catch (const std::exception& error) {
            rejectSelection(error);
            return false;
        }
    }

    void ActivityAnalyticsViewModel::cancel() {
        if (!canCancel()) {
            return;
        }
        const auto found = std::ranges::find_if(operations_, [this](const auto& operation) {
            return operation->id == latestOperationId_ && !operation->completed;
        });
        if (found == operations_.end()) {
            return;
        }
        (*found)->explicitlyCanceled = true;
        setState(true, true);
        stop(**found);
    }

    void ActivityAnalyticsViewModel::replaceService(
        std::shared_ptr<const application::ActivityAnalyticsService> service) {
        if (!service) {
            throw std::invalid_argument("activity analytics service is required");
        }
        service_ = std::move(service);
        invalidate(true);
    }

    void ActivityAnalyticsViewModel::invalidateAfterImport() {
        invalidate(false);
    }

    void ActivityAnalyticsViewModel::start(const RequestKind kind, Work work) {
        if (shuttingDown_) {
            return;
        }
        const bool replacing = hasActiveOperations();
        stopAll();

        auto operation = std::make_unique<Operation>();
        operation->id = ++nextOperationId_;
        operation->generation = generation_;
        operation->kind = kind;
        operation->state = std::make_shared<TaskState>();
        const auto operationId = operation->id;
        const auto state = operation->state;
        const auto stopToken = operation->stopSource.get_token();
        connect(&operation->watcher, &QFutureWatcher<void>::finished, this,
                [this, operationId] { finish(operationId); });
        auto* watcher = &operation->watcher;
        latestOperationId_ = operationId;
        operations_.push_back(std::move(operation));
        emit activeOperationsChanged();
        if (replacing) {
            emit replaced();
        }
        setErrorMessage({});
        setState(true, false);

        watcher->setFuture(QtConcurrent::run(
            QThreadPool::globalInstance(), [state, stopToken, work = std::move(work)]() mutable {
                try {
                    auto result = work(stopToken);
                    const std::scoped_lock lock(state->mutex);
                    state->result = std::move(result);
                    state->canceled = stopToken.stop_requested();
                } catch (const std::system_error& error) {
                    const std::scoped_lock lock(state->mutex);
                    if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                        state->canceled = true;
                    } else {
                        state->error = std::current_exception();
                    }
                } catch (...) {
                    const std::scoped_lock lock(state->mutex);
                    state->error = std::current_exception();
                }
            }));
    }

    void ActivityAnalyticsViewModel::finish(const std::uint64_t operationId) {
        const auto found = std::ranges::find_if(operations_, [operationId](const auto& operation) {
            return operation->id == operationId;
        });
        if (found == operations_.end()) {
            return;
        }
        auto& operation = **found;
        operation.completed = true;
        emit activeOperationsChanged();

        std::optional<TaskResult> result;
        std::exception_ptr error;
        bool taskCanceled = false;
        {
            const std::scoped_lock lock(operation.state->mutex);
            result = std::move(operation.state->result);
            error = operation.state->error;
            taskCanceled = operation.state->canceled;
        }

        const bool mayPublish = !shuttingDown_ && operation.id == latestOperationId_ &&
                                operation.generation == generation_;
        std::optional<bool> warningWindowLoadSucceeded;
        if (mayPublish) {
            const bool canceledOperation = operation.explicitlyCanceled || taskCanceled ||
                                           operation.stopSource.stop_requested();
            if (canceledOperation) {
                setErrorMessage({});
                setState(false, false);
                emit canceled();
            } else if (error) {
                const auto message = exceptionMessage(error);
                qWarning().noquote() << "Activity analytics query failed:" << message;
                setErrorMessage(message);
                setState(false, false);
                emit failed(message);
                if (operation.kind == RequestKind::WarningWindowLoad) {
                    warningWindowLoadSucceeded = false;
                }
            } else if (!result) {
                const auto message = QStringLiteral("A consulta analitica nao retornou resultado");
                qWarning().noquote() << message;
                setErrorMessage(message);
                setState(false, false);
                emit failed(message);
                if (operation.kind == RequestKind::WarningWindowLoad) {
                    warningWindowLoadSucceeded = false;
                }
            } else {
                publish(operation, std::move(*result));
                setState(false, false);
                emit succeeded(operation.kind);
                if (operation.kind == RequestKind::WarningWindowLoad) {
                    warningWindowLoadSucceeded = true;
                }
            }
        }
        if (warningWindowLoadSucceeded.has_value()) {
            emit warningWindowLoadFinished(*warningWindowLoadSucceeded);
        }
        QMetaObject::invokeMethod(this, [this] { pruneCompleted(); }, Qt::QueuedConnection);
    }

    void ActivityAnalyticsViewModel::stop(Operation& operation) {
        operation.stopSource.request_stop();
        operation.watcher.cancel();
    }

    void ActivityAnalyticsViewModel::stopAll() {
        for (const auto& operation : operations_) {
            if (!operation->completed) {
                stop(*operation);
            }
        }
    }

    void ActivityAnalyticsViewModel::pruneCompleted() {
        std::erase_if(operations_, [](const auto& operation) { return operation->completed; });
    }

    void ActivityAnalyticsViewModel::publish(const Operation& operation, TaskResult result) {
        switch (operation.kind) {
        case RequestKind::Dashboard: {
            const auto& value = std::get<DashboardPayload>(result);
            dashboard_ = value.model;
            emit dashboardChanged();
            for (const auto& unavailable : value.unavailableMetrics) {
                emit metricUnavailable(static_cast<int>(unavailable.metric), unavailable.reason);
            }
            break;
        }
        case RequestKind::CustomSeries: {
            const auto& value = std::get<CustomSeriesPayload>(result);
            customSeries_ = value.model;
            emit customSeriesChanged();
            if (value.unavailable) {
                emit metricUnavailable(static_cast<int>(value.metric), value.unavailableReason);
            }
            break;
        }
        case RequestKind::DimensionValues:
            dimensionValues_ = dimensionMap(std::get<domain::AnalyticsDimensionValues>(result));
            emit dimensionValuesChanged();
            break;
        case RequestKind::Availability: {
            const auto& values = std::get<std::vector<domain::AnalyticsMetricAvailability>>(result);
            availability_ = availabilityList(values);
            emit availabilityChanged();
            for (const auto& value : values) {
                if (!value.available) {
                    emit metricUnavailable(static_cast<int>(value.metric),
                                           QString::fromStdString(value.reason));
                }
            }
            break;
        }
        case RequestKind::WarningWindowLoad:
        case RequestKind::WarningWindowSave: {
            const auto& value = std::get<WarningWindowPayload>(result);
            const auto next = optionalInt(value.days);
            if (warningWindowDays_ != next) {
                warningWindowDays_ = next;
                emit warningWindowDaysChanged();
            }
            break;
        }
        }
    }

    void ActivityAnalyticsViewModel::clearModels(const bool clearSettings) {
        if (!dashboard_.isEmpty()) {
            dashboard_.clear();
            emit dashboardChanged();
        }
        if (!customSeries_.isEmpty()) {
            customSeries_.clear();
            emit customSeriesChanged();
        }
        if (!dimensionValues_.isEmpty()) {
            dimensionValues_.clear();
            emit dimensionValuesChanged();
        }
        if (!availability_.isEmpty()) {
            availability_.clear();
            emit availabilityChanged();
        }
        if (clearSettings && warningWindowDays_.isValid()) {
            warningWindowDays_.clear();
            emit warningWindowDaysChanged();
        }
    }

    void ActivityAnalyticsViewModel::invalidate(const bool clearSettings) {
        ++generation_;
        latestOperationId_ = 0;
        stopAll();
        setErrorMessage({});
        setState(false, false);
        clearModels(clearSettings);
        emit invalidated();
    }

    void ActivityAnalyticsViewModel::rejectSelection(const std::exception& error) {
        const auto detail = QString::fromUtf8(error.what()).trimmed();
        const auto message = detail.isEmpty()
                                 ? QStringLiteral("Selecao de analise invalida")
                                 : QStringLiteral("Selecao de analise invalida: %1").arg(detail);
        setErrorMessage(message);
        emit failed(message);
    }

    void ActivityAnalyticsViewModel::setState(const bool loading, const bool canceling) {
        if (loading_ == loading && canceling_ == canceling) {
            return;
        }
        loading_ = loading;
        canceling_ = canceling;
        emit stateChanged();
    }

    void ActivityAnalyticsViewModel::setErrorMessage(QString message) {
        if (errorMessage_ == message) {
            return;
        }
        errorMessage_ = std::move(message);
        emit errorMessageChanged();
    }

} // namespace ssa::presentation
