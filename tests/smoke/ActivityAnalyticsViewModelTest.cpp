#include "presentation/ActivityAnalyticsViewModel.h"
#include "application/ActivityAnalyticsService.h"
#include "domain/ActivityAnalyticsTypes.h"
#include "ports/IActivityAnalyticsPort.h"
#include "ports/IActivityAnalyticsSettingsPort.h"

#include <QDate>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <system_error>
#include <utility>
#include <vector>

namespace {

    using ssa::application::ActivityAnalyticsService;
    using ssa::domain::AnalyticsDimensionValues;
    using ssa::domain::AnalyticsMetricAvailability;
    using ssa::domain::AnalyticsPeriod;
    using ssa::domain::AnalyticsPoint;
    using ssa::domain::AnalyticsRequest;
    using ssa::domain::AnalyticsSeriesResult;
    using ssa::domain::IsoWeek;

    AnalyticsPeriod testPeriod() {
        return {.first = IsoWeek{2026, 1}, .last = IsoWeek{2026, 13}};
    }

    AnalyticsPeriod testHistoryPeriod() {
        return {.first = IsoWeek{2025, 14}, .last = IsoWeek{2026, 13}};
    }

    AnalyticsRequest testRequest(const std::string& division = "DIV") {
        return {.metric = ssa::domain::AnalyticsMetric::Executed,
                .period = testPeriod(),
                .grain = ssa::domain::TimeGrain::WholePeriod,
                .breakdown = ssa::domain::Breakdown::Division,
                .personRole = ssa::domain::PersonRole::Executor,
                .divisions = {division}};
    }

    AnalyticsSeriesResult seriesResult(const std::int64_t count,
                                       const std::string& division = "DIV") {
        return {.points = {AnalyticsPoint{.division = division, .count = count}},
                .sourceRevision = "revision-1",
                .observedIsoYearWeek = 202613};
    }

    AnalyticsSeriesResult dashboardSeriesResult(const AnalyticsRequest& request,
                                                const std::int64_t count) {
        auto result = seriesResult(count);
        auto& point = result.points.front();
        switch (request.grain) {
        case ssa::domain::TimeGrain::WholePeriod:
            break;
        case ssa::domain::TimeGrain::IsoWeek:
            point.bucketKey = "2026-W13";
            break;
        case ssa::domain::TimeGrain::IsoReferenceMonth:
            point.bucketKey = "2026-03";
            break;
        }
        if (request.metric == ssa::domain::AnalyticsMetric::PendingDeadline) {
            point.deadlineClass = ssa::domain::DeadlineClass::OnTime;
        }
        return result;
    }

    class FunctionalAnalyticsPort final : public ssa::ports::IActivityAnalyticsPort {
      public:
        using SeriesFunction =
            std::function<AnalyticsSeriesResult(const AnalyticsRequest&, std::stop_token)>;
        using DimensionFunction =
            std::function<AnalyticsDimensionValues(const AnalyticsRequest&, std::stop_token)>;
        using AvailabilityFunction =
            std::function<std::vector<AnalyticsMetricAvailability>(std::stop_token)>;

        SeriesFunction seriesFunction = [](const AnalyticsRequest&, const std::stop_token&) {
            return seriesResult(1);
        };
        DimensionFunction dimensionFunction = [](const AnalyticsRequest&, const std::stop_token&) {
            return AnalyticsDimensionValues{};
        };
        AvailabilityFunction availabilityFunction = [](const std::stop_token&) {
            return std::vector<AnalyticsMetricAvailability>{};
        };

        AnalyticsSeriesResult series(const AnalyticsRequest& request,
                                     std::stop_token stopToken) const override {
            return seriesFunction(request, stopToken);
        }

        AnalyticsDimensionValues dimensionValues(const AnalyticsRequest& request,
                                                 std::stop_token stopToken) const override {
            return dimensionFunction(request, stopToken);
        }

        std::vector<AnalyticsMetricAvailability>
        availability(std::stop_token stopToken) const override {
            return availabilityFunction(stopToken);
        }
    };

    class FunctionalSettingsPort final : public ssa::ports::IActivityAnalyticsSettingsPort {
      public:
        using LoadFunction = std::function<std::optional<int>(std::stop_token)>;
        using SaveFunction = std::function<void(int, std::stop_token)>;

        LoadFunction loadFunction = [](const std::stop_token&) { return std::optional<int>{}; };
        SaveFunction saveFunction = [](int, const std::stop_token&) {};

        std::optional<int> warningWindowDays(std::stop_token stopToken) const override {
            return loadFunction(stopToken);
        }

        void setWarningWindowDays(const int days, std::stop_token stopToken) override {
            saveFunction(days, stopToken);
        }
    };

    std::shared_ptr<const ActivityAnalyticsService>
    serviceFor(const std::shared_ptr<FunctionalAnalyticsPort>& port,
               std::shared_ptr<FunctionalSettingsPort> settings = nullptr) {
        return std::make_shared<ActivityAnalyticsService>(port, std::move(settings));
    }

    double firstChartValue(const QVariantMap& result) {
        const auto series = result.value(QStringLiteral("series")).toList();
        if (series.empty()) {
            return -1;
        }
        const auto values = series.front().toMap().value(QStringLiteral("values")).toList();
        return values.empty() ? -1 : values.front().toDouble();
    }

    class ActivityAnalyticsViewModelTest final : public QObject {
        Q_OBJECT

      private slots:
        void currentMonthSelectionUsesLastCompleteCalendarMonth() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QDate expectedMonth = QDate::currentDate().addMonths(-1);
            const auto expectedPeriod =
                ssa::domain::calendarMonthPeriod(expectedMonth.year(), expectedMonth.month());

            const QVariantMap selection = model.currentMonthSelection();

            QCOMPARE(selection.value(QStringLiteral("year")).toInt(), expectedMonth.year());
            QCOMPARE(selection.value(QStringLiteral("month")).toInt(), expectedMonth.month());
            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(),
                     expectedPeriod.first.year);
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(),
                     expectedPeriod.first.week);
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(), expectedPeriod.last.year);
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(), expectedPeriod.last.week);
        }

        void calendarMonthSelectionMapsRequestedMonthToIsoWeeks() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));

            const QVariantMap selection = model.calendarMonthSelection(2026, 7);

            QCOMPARE(selection.value(QStringLiteral("year")).toInt(), 2026);
            QCOMPARE(selection.value(QStringLiteral("month")).toInt(), 7);
            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(), 2026);
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(), 27);
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(), 2026);
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(), 31);
        }

        void yearToDateSelectionUsesFirstIsoWeekThroughCurrentWeek() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QDate today = QDate::currentDate();
            const auto expected =
                ssa::domain::yearToDateCalendarSelection(
                    today.toString(Qt::ISODate).toStdString());
            QVERIFY(expected.has_value());

            const QVariantMap selection = model.yearToDateSelection();

            QCOMPARE(selection.value(QStringLiteral("year")).toInt(), expected->first.year);
            QCOMPARE(selection.value(QStringLiteral("month")).toInt(), 0);
            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(), expected->first.year);
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(), expected->first.week);
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(), expected->last.year);
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(), expected->last.week);
        }

        void yearToDateSelectionUsesIsoReferenceMonthAtYearBoundary() {
            const auto selection =
                ssa::domain::yearToDateCalendarSelection("2024-12-31");
            QVERIFY(selection.has_value());
            QCOMPARE(selection->year, 2025);
            QCOMPARE(selection->month, 1);
            QCOMPARE(selection->first.year, 2025);
            QCOMPARE(selection->first.week, 1);
            QCOMPARE(selection->last.year, 2025);
            QCOMPARE(selection->last.week, 1);
        }

        void clearCustomSeriesEmitsChangeAndClearsModel() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy customSpy(
                &model, &ssa::presentation::ActivityAnalyticsViewModel::customSeriesChanged);

            QVERIFY(model.requestCustomSeries({{QStringLiteral("metric"), 0},
                                               {QStringLiteral("firstYear"), 2026},
                                               {QStringLiteral("firstWeek"), 1},
                                               {QStringLiteral("lastYear"), 2026},
                                               {QStringLiteral("lastWeek"), 13},
                                               {QStringLiteral("grain"), 0},
                                               {QStringLiteral("breakdown"), 0},
                                               {QStringLiteral("personRole"), 2}}));
            QTRY_COMPARE_WITH_TIMEOUT(customSpy.count(), 1, 5000);
            QVERIFY(!model.customSeries().isEmpty());

            model.clearCustomSeries();
            QCOMPARE(customSpy.count(), 2);
            QVERIFY(model.customSeries().isEmpty());
        }

        void currentIsoWeekSelectionUsesCurrentIsoWeek() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QDate today = QDate::currentDate();
            int isoYear = 0;
            const int isoWeek = today.weekNumber(&isoYear);

            const QVariantMap selection = model.currentIsoWeekSelection();

            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(), isoYear);
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(), isoWeek);
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(), isoYear);
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(), isoWeek);
        }

        void customChartTitleBuildsAdaptiveLabel() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QVariantMap selection{
                {QStringLiteral("metric"), 1},
                {QStringLiteral("firstYear"), 2026},
                {QStringLiteral("firstWeek"), 27},
                {QStringLiteral("lastYear"), 2026},
                {QStringLiteral("lastWeek"), 31},
                {QStringLiteral("grain"), 0},
                {QStringLiteral("breakdown"), 2},
                {QStringLiteral("personRole"), 2},
            };

            const QString title = model.customChartTitle(selection);

            QCOMPARE(title, QStringLiteral("SSAs executadas de 202627 a 202631 em Divisao e "
                                           "pessoa (Execucao)"));
        }

        void writeExportFilePersistsContent() {
            const auto port = std::make_shared<FunctionalAnalyticsPort>();
            const ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QTemporaryFile file;
            QVERIFY(file.open());
            const QString path = file.fileName();
            file.close();

            QVERIFY(model.writeExportFile(path, QStringLiteral("<svg></svg>")));
            QFile reader(path);
            QVERIFY(reader.open(QIODevice::ReadOnly));
            QCOMPARE(QString::fromUtf8(reader.readAll()), QStringLiteral("<svg></svg>"));
        }

        void rejectsMissingService() {
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                     ssa::presentation::ActivityAnalyticsViewModel(nullptr));
        }

        void customAnalysisStartsImmediatelyAndOnlyLatestPublishes() {
            struct Gate final {
                std::atomic_int calls{0};
                std::atomic_bool firstStarted{false};
                std::atomic_bool secondStarted{false};
                std::atomic_bool firstStopObserved{false};
                std::atomic_bool firstFinished{false};
                std::atomic_bool releaseFirst{false};
            };
            const auto gate = std::make_shared<Gate>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [gate](const AnalyticsRequest&, const std::stop_token& token) {
                const int call = ++gate->calls;
                if (call == 1) {
                    gate->firstStarted = true;
                    while (!gate->releaseFirst.load()) {
                        gate->firstStopObserved = token.stop_requested();
                        QThread::msleep(1);
                    }
                    gate->firstFinished = true;
                    return seriesResult(1, "OLD");
                }
                gate->secondStarted = true;
                return seriesResult(2, "NEW");
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy succeededSpy(&model,
                                    &ssa::presentation::ActivityAnalyticsViewModel::succeeded);
            QSignalSpy replacedSpy(&model,
                                   &ssa::presentation::ActivityAnalyticsViewModel::replaced);

            QElapsedTimer firstCall;
            firstCall.start();
            model.loadCustomSeries(testRequest("OLD"));

            QVERIFY(firstCall.elapsed() < 100);
            QVERIFY(model.loading());
            QTRY_VERIFY_WITH_TIMEOUT(gate->firstStarted.load(), 1000);

            QElapsedTimer replacementCall;
            replacementCall.start();
            model.loadCustomSeries(testRequest("NEW"));

            QVERIFY(replacementCall.elapsed() < 100);
            QTRY_VERIFY_WITH_TIMEOUT(gate->secondStarted.load(), 1000);
            QVERIFY(!gate->firstFinished.load());
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(firstChartValue(model.customSeries()), 2.0);
            QCOMPARE(succeededSpy.count(), 1);
            QCOMPARE(replacedSpy.count(), 1);
            QTRY_VERIFY_WITH_TIMEOUT(gate->firstStopObserved.load(), 1000);

            gate->releaseFirst = true;
            QTRY_VERIFY_WITH_TIMEOUT(gate->firstFinished.load(), 1000);
            QTest::qWait(20);
            QCOMPARE(firstChartValue(model.customSeries()), 2.0);
            QCOMPARE(succeededSpy.count(), 1);
        }

        void cancelIsImmediateAndTerminalOnlyAfterWorkerStops() {
            struct Gate final {
                std::atomic_bool started{false};
                std::atomic_bool stopObserved{false};
            };
            const auto gate = std::make_shared<Gate>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [gate](const AnalyticsRequest&,
                                          const std::stop_token& token) -> AnalyticsSeriesResult {
                gate->started = true;
                while (!token.stop_requested()) {
                    QThread::msleep(1);
                }
                gate->stopObserved = true;
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy canceledSpy(&model,
                                   &ssa::presentation::ActivityAnalyticsViewModel::canceled);
            QSignalSpy failedSpy(&model, &ssa::presentation::ActivityAnalyticsViewModel::failed);

            model.loadCustomSeries(testRequest());
            QTRY_VERIFY_WITH_TIMEOUT(gate->started.load(), 1000);
            QElapsedTimer timer;
            timer.start();

            model.cancel();
            model.cancel();

            QVERIFY(timer.elapsed() < 50);
            QVERIFY(model.loading());
            QVERIFY(model.canceling());
            QVERIFY(!model.canCancel());
            QTRY_VERIFY_WITH_TIMEOUT(gate->stopObserved.load(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QVERIFY(!model.canceling());
            QCOMPARE(canceledSpy.count(), 1);
            QCOMPARE(failedSpy.count(), 0);
            QVERIFY(model.errorMessage().isEmpty());
            QVERIFY(model.customSeries().isEmpty());
        }

        void terminalSignalsNeverExposeCancelableWithoutActiveWork() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            bool incoherentStateObserved = false;
            connect(&model, &ssa::presentation::ActivityAnalyticsViewModel::activeOperationsChanged,
                    this, [&model, &incoherentStateObserved] {
                        incoherentStateObserved =
                            incoherentStateObserved ||
                            (!model.hasActiveOperations() && model.canCancel());
                    });

            model.loadCustomSeries(testRequest());

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QVERIFY(!model.hasActiveOperations());
            QVERIFY(!model.canCancel());
            QVERIFY(!incoherentStateObserved);
        }

        void destructionIsNonBlockingAndKeepsDependenciesAlive() {
            struct Gate final {
                std::atomic_bool started{false};
                std::atomic_bool stopObserved{false};
                std::atomic_bool finished{false};
            };
            const auto gate = std::make_shared<Gate>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [gate](const AnalyticsRequest&,
                                          const std::stop_token& token) -> AnalyticsSeriesResult {
                gate->started = true;
                while (!token.stop_requested()) {
                    QThread::msleep(1);
                }
                gate->stopObserved = true;
                gate->finished = true;
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            };
            auto model =
                std::make_unique<ssa::presentation::ActivityAnalyticsViewModel>(serviceFor(port));
            model->loadCustomSeries(testRequest());
            QTRY_VERIFY_WITH_TIMEOUT(gate->started.load(), 1000);
            QElapsedTimer timer;
            timer.start();

            model.reset();

            QVERIFY(timer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(gate->stopObserved.load(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(gate->finished.load(), 1000);
        }

        void failurePublishesObjectiveErrorWithoutData() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest&,
                                      const std::stop_token&) -> AnalyticsSeriesResult {
                throw std::runtime_error("analytics exploded");
                return AnalyticsSeriesResult{};
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy failedSpy(&model, &ssa::presentation::ActivityAnalyticsViewModel::failed);

            model.loadCustomSeries(testRequest());

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(model.errorMessage(), QStringLiteral("analytics exploded"));
            QCOMPARE(failedSpy.count(), 1);
            QVERIFY(model.customSeries().isEmpty());
        }

        void unavailableMetricPublishesCompleteQmlSafeMetadata() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest&, const std::stop_token&) {
                auto result = seriesResult(7);
                result.points.front().bucketKey = "2026-W13";
                result.points.front().sector = "DIV-01";
                result.points.front().person = "Person";
                result.points.front().cohort =
                    ssa::domain::RegistrationCohort::RegisteredBeforePeriod;
                result.points.front().deadlineClass = ssa::domain::DeadlineClass::Warning;
                result.excludedForDataQuality = 3;
                result.complete = false;
                result.unavailableReason = "source is incomplete";
                return result;
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy unavailableSpy(
                &model, &ssa::presentation::ActivityAnalyticsViewModel::metricUnavailable);

            model.loadCustomSeries(testRequest());

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(unavailableSpy.count(), 1);
            QCOMPARE(unavailableSpy.front().at(0).toInt(),
                     static_cast<int>(ssa::domain::AnalyticsMetric::Executed));
            QCOMPARE(unavailableSpy.front().at(1).toString(),
                     QStringLiteral("source is incomplete"));
            const auto result = model.customSeries();
            QCOMPARE(result.value(QStringLiteral("sourceRevision")).toString(),
                     QStringLiteral("revision-1"));
            QCOMPARE(result.value(QStringLiteral("observedIsoYearWeek")).toInt(), 202613);
            QCOMPARE(result.value(QStringLiteral("excludedForDataQuality")).toLongLong(), 3);
            QCOMPARE(result.value(QStringLiteral("complete")).toBool(), false);
            QCOMPARE(result.value(QStringLiteral("available")).toBool(), false);
            QCOMPARE(result.value(QStringLiteral("unavailableReason")).toString(),
                     QStringLiteral("source is incomplete"));
            QVERIFY(result.value(QStringLiteral("series")).toList().isEmpty());
            QVERIFY(!result.contains(QStringLiteral("points")));
            QVERIFY(!result.contains(QStringLiteral("observations")));
        }

        void dashboardPublishesFourteenNamedChartModels() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest& request, const std::stop_token&) {
                return dashboardSeriesResult(request, static_cast<int>(request.metric) + 1);
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy changedSpy(&model,
                                  &ssa::presentation::ActivityAnalyticsViewModel::dashboardChanged);

            model.loadDashboard(testPeriod(), testHistoryPeriod(), 30);

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(changedSpy.count(), 1);
            QCOMPARE(model.dashboard().size(), 14);
            QVERIFY(model.dashboard().contains(QStringLiteral("registeredBySector")));
            QVERIFY(model.dashboard().contains(QStringLiteral("pendingDeadlinePercentage")));
            QVERIFY(model.dashboard().contains(QStringLiteral("pendingDeadlineQuantity")));
            QCOMPARE(model.dashboard()
                         .value(QStringLiteral("pendingDeadlinePercentage"))
                         .toMap()
                         .value(QStringLiteral("percentage"))
                         .toBool(),
                     true);
            QCOMPARE(model.dashboard()
                         .value(QStringLiteral("pendingDeadlineQuantity"))
                         .toMap()
                         .value(QStringLiteral("percentage"))
                         .toBool(),
                     false);
            for (const auto& modelValue : model.dashboard()) {
                const auto chart = modelValue.toMap();
                QVERIFY(chart.contains(QStringLiteral("categories")));
                QVERIFY(chart.contains(QStringLiteral("series")));
                QVERIFY(!chart.contains(QStringLiteral("points")));
                QVERIFY(!chart.contains(QStringLiteral("observations")));
            }
        }

        void qmlBoundaryBuildsAndValidatesCustomRequest() {
            struct Capture final {
                std::mutex mutex;
                std::optional<AnalyticsRequest> request;
                std::atomic_int calls{0};
            };
            const auto capture = std::make_shared<Capture>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [capture](const AnalyticsRequest& request,
                                             const std::stop_token&) {
                const std::scoped_lock lock(capture->mutex);
                capture->request = request;
                ++capture->calls;
                auto result = seriesResult(4);
                result.points.front().bucketKey = "2026-W02";
                result.points.front().sector = "DIV-01";
                result.points.front().person = "Person";
                result.points.front().deadlineClass = ssa::domain::DeadlineClass::OnTime;
                return result;
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QVariantMap selection{
                {QStringLiteral("metric"),
                 static_cast<int>(ssa::domain::AnalyticsMetric::PendingDeadline)},
                {QStringLiteral("firstYear"), 2026},
                {QStringLiteral("firstWeek"), 2},
                {QStringLiteral("lastYear"), 2026},
                {QStringLiteral("lastWeek"), 12},
                {QStringLiteral("grain"), static_cast<int>(ssa::domain::TimeGrain::IsoWeek)},
                {QStringLiteral("breakdown"),
                 static_cast<int>(ssa::domain::Breakdown::DivisionSectorPerson)},
                {QStringLiteral("personRole"), static_cast<int>(ssa::domain::PersonRole::Planner)},
                {QStringLiteral("divisions"), QStringList{QStringLiteral("DIV")}},
                {QStringLiteral("sectors"), QStringList{QStringLiteral("DIV-01")}},
                {QStringLiteral("people"), QStringList{QStringLiteral("Person")}},
                {QStringLiteral("warningWindowDays"), 21},
            };
            bool accepted = false;

            const bool invoked = QMetaObject::invokeMethod(
                &model, "requestCustomSeries", Qt::DirectConnection, Q_RETURN_ARG(bool, accepted),
                Q_ARG(QVariantMap, selection));

            QVERIFY(invoked);
            QVERIFY(accepted);
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            std::optional<AnalyticsRequest> request;
            {
                const std::scoped_lock lock(capture->mutex);
                request = capture->request;
            }
            QVERIFY(request.has_value());
            if (!request) {
                return;
            }
            const auto captured = *request;
            QCOMPARE(captured.metric, ssa::domain::AnalyticsMetric::PendingDeadline);
            QCOMPARE(captured.period.first, (IsoWeek{2026, 2}));
            QCOMPARE(captured.period.last, (IsoWeek{2026, 12}));
            QCOMPARE(captured.grain, ssa::domain::TimeGrain::IsoWeek);
            QCOMPARE(captured.breakdown, ssa::domain::Breakdown::DivisionSectorPerson);
            QCOMPARE(captured.personRole, ssa::domain::PersonRole::Planner);
            QCOMPARE(captured.divisions, std::vector<std::string>{"DIV"});
            QCOMPARE(captured.sectors, std::vector<std::string>{"DIV-01"});
            QCOMPARE(captured.people, std::vector<std::string>{"Person"});
            QCOMPARE(captured.warningWindowDays, std::optional<int>{21});

            auto invalidSelection = selection;
            invalidSelection.insert(QStringLiteral("metric"), 99);
            accepted = true;
            QVERIFY(QMetaObject::invokeMethod(&model, "requestCustomSeries", Qt::DirectConnection,
                                              Q_RETURN_ARG(bool, accepted),
                                              Q_ARG(QVariantMap, invalidSelection)));
            QVERIFY(!accepted);
            QCOMPARE(capture->calls.load(), 1);
            QVERIFY(!model.errorMessage().isEmpty());
            QVERIFY(model.metaObject()->indexOfMethod("requestDashboard(QVariantMap)") >= 0);
            QVERIFY(model.metaObject()->indexOfMethod("requestDimensionValues(QVariantMap)") >= 0);
        }

        void qmlDashboardBoundaryUsesConventionalPeriodKeys() {
            struct Capture final {
                std::mutex mutex;
                std::vector<AnalyticsRequest> requests;
            };
            const auto capture = std::make_shared<Capture>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [capture](const AnalyticsRequest& request,
                                             const std::stop_token&) {
                const std::scoped_lock lock(capture->mutex);
                capture->requests.push_back(request);
                return dashboardSeriesResult(request, 1);
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            const QVariantMap selection{
                {QStringLiteral("reportFirstYear"), 2026}, {QStringLiteral("reportFirstWeek"), 9},
                {QStringLiteral("reportLastYear"), 2026},  {QStringLiteral("reportLastWeek"), 13},
                {QStringLiteral("warningWindowDays"), 30},
            };
            bool accepted = false;

            QVERIFY(QMetaObject::invokeMethod(&model, "requestDashboard", Qt::DirectConnection,
                                              Q_RETURN_ARG(bool, accepted),
                                              Q_ARG(QVariantMap, selection)));

            QVERIFY(accepted);
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            std::vector<AnalyticsRequest> requests;
            {
                const std::scoped_lock lock(capture->mutex);
                requests = capture->requests;
            }
            QCOMPARE(requests.size(), std::size_t{13});
            QCOMPARE(requests.at(0).period,
                     (AnalyticsPeriod{.first = {2026, 9}, .last = {2026, 13}}));
            QCOMPARE(requests.at(1).period,
                     ssa::domain::referenceMonthHistoryPeriod(IsoWeek{2026, 13}, 13));
        }

        void customResultIsChartReadyWithoutQmlAggregation() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest&, const std::stop_token&) {
                return AnalyticsSeriesResult{
                    .points = {AnalyticsPoint{.division = "DIV", .count = 2},
                               AnalyticsPoint{.division = "DIV", .count = 3}},
                    .sourceRevision = "revision-2",
                    .observedIsoYearWeek = 202613};
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));

            model.loadCustomSeries(testRequest());

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            const auto result = model.customSeries();
            QCOMPARE(result.value(QStringLiteral("categories")).toStringList(),
                     QStringList{QStringLiteral("DIV")});
            const auto chartSeries = result.value(QStringLiteral("series")).toList();
            QCOMPARE(chartSeries.size(), 1);
            QCOMPARE(chartSeries.front().toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("total"));
            QCOMPARE(chartSeries.front()
                         .toMap()
                         .value(QStringLiteral("values"))
                         .toList()
                         .front()
                         .toDouble(),
                     5.0);
            QCOMPARE(result.value(QStringLiteral("total")).toDouble(), 5.0);
            QVERIFY(result.contains(QStringLiteral("qualityText")));
            QVERIFY(result.contains(QStringLiteral("percentage")));
            QVERIFY(!result.contains(QStringLiteral("points")));
            QVERIFY(!result.contains(QStringLiteral("observations")));
        }

        void customDeadlineResultKeepsDeadlineClassesByArea() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest&, const std::stop_token&) {
                auto onTime = AnalyticsPoint{.division = "DIV", .sector = "AREA-A", .count = 5};
                onTime.deadlineClass = ssa::domain::DeadlineClass::OnTime;
                auto warning = AnalyticsPoint{.division = "DIV", .sector = "AREA-A", .count = 2};
                warning.deadlineClass = ssa::domain::DeadlineClass::Warning;
                auto overdue = AnalyticsPoint{.division = "DIV", .sector = "AREA-B", .count = 3};
                overdue.deadlineClass = ssa::domain::DeadlineClass::Overdue;
                return AnalyticsSeriesResult{.points = {onTime, warning, overdue}};
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            auto request = testRequest();
            request.metric = ssa::domain::AnalyticsMetric::PendingDeadline;
            request.grain = ssa::domain::TimeGrain::WholePeriod;
            request.breakdown = ssa::domain::Breakdown::DivisionSector;
            request.warningWindowDays = 7;

            model.loadCustomSeries(request);

            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            const auto result = model.customSeries();
            QCOMPARE(result.value(QStringLiteral("categories")).toStringList(),
                     QStringList({QStringLiteral("DIV / AREA-A"), QStringLiteral("DIV / AREA-B")}));
            const auto series = result.value(QStringLiteral("series")).toList();
            QCOMPARE(series.size(), 3);
            QCOMPARE(series.at(0).toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("on_time"));
            QCOMPARE(series.at(1).toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("warning"));
            QCOMPARE(series.at(2).toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("overdue"));
            QCOMPARE(series.at(2).toMap().value(QStringLiteral("values")).toList().at(1).toDouble(),
                     3.0);
        }

        void warningWindowSettingsAreAsyncValidatedAndQmlVisible() {
            struct Gate final {
                std::atomic_bool started{false};
                std::atomic_bool release{false};
                std::atomic_int saved{-1};
            };
            const auto gate = std::make_shared<Gate>();
            auto settings = std::make_shared<FunctionalSettingsPort>();
            settings->loadFunction = [gate](const std::stop_token& token) {
                gate->started = true;
                while (!gate->release.load() && !token.stop_requested()) {
                    QThread::msleep(1);
                }
                return std::optional<int>{14};
            };
            settings->saveFunction = [gate](const int days, const std::stop_token&) {
                gate->saved = days;
            };
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port, settings));
            QSignalSpy changedSpy(
                &model, &ssa::presentation::ActivityAnalyticsViewModel::warningWindowDaysChanged);
            QSignalSpy loadFinishedSpy(
                &model, &ssa::presentation::ActivityAnalyticsViewModel::warningWindowLoadFinished);
            QElapsedTimer timer;
            timer.start();

            model.loadWarningWindowDays();

            QVERIFY(timer.elapsed() < 100);
            QVERIFY(model.loading());
            QTRY_VERIFY_WITH_TIMEOUT(gate->started.load(), 1000);
            gate->release = true;
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(model.warningWindowDays().toInt(), 14);
            QVERIFY(changedSpy.count() >= 1);
            QCOMPARE(loadFinishedSpy.count(), 1);
            QCOMPARE(loadFinishedSpy.front().front().toBool(), true);

            QVERIFY(model.saveWarningWindowDays(21));
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(gate->saved.load(), 21);
            QCOMPARE(model.warningWindowDays().toInt(), 21);
            const auto changedAfterSave = changedSpy.count();

            QVERIFY(!model.saveWarningWindowDays(366));
            QCOMPARE(gate->saved.load(), 21);
            QCOMPARE(changedSpy.count(), changedAfterSave);
            QVERIFY(!model.errorMessage().isEmpty());

            settings->loadFunction = [](const std::stop_token&) -> std::optional<int> {
                throw std::runtime_error("settings unavailable");
            };
            model.loadWarningWindowDays();
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(loadFinishedSpy.count(), 2);
            QCOMPARE(loadFinishedSpy.back().front().toBool(), false);
        }

        void dimensionsAndAvailabilityUseQmlSafeModels() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->dimensionFunction = [](const AnalyticsRequest&, const std::stop_token&) {
                return AnalyticsDimensionValues{
                    .divisions = {"DIV"}, .sectors = {"DIV-01"}, .people = {"Person"}};
            };
            port->availabilityFunction = [](const std::stop_token&) {
                return std::vector<AnalyticsMetricAvailability>{
                    {.metric = ssa::domain::AnalyticsMetric::Executed,
                     .firstIsoYearWeek = 202601,
                     .lastIsoYearWeek = 202613,
                     .available = true}};
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));

            model.loadDimensionValues(testRequest());
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(model.dimensionValues().value(QStringLiteral("divisions")).toStringList(),
                     QStringList{QStringLiteral("DIV")});
            QCOMPARE(model.dimensionValues().value(QStringLiteral("people")).toStringList(),
                     QStringList{QStringLiteral("Person")});

            model.loadAvailability();
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(model.availability().size(), 1);
            const auto availability = model.availability().front().toMap();
            QCOMPARE(availability.value(QStringLiteral("metric")).toInt(),
                     static_cast<int>(ssa::domain::AnalyticsMetric::Executed));
            QCOMPARE(availability.value(QStringLiteral("firstIsoYearWeek")).toInt(), 202601);
            QCOMPARE(availability.value(QStringLiteral("available")).toBool(), true);
        }

        void replacingServiceInvalidatesOldDatabaseResults() {
            struct Gate final {
                std::atomic_bool started{false};
                std::atomic_bool release{false};
            };
            const auto gate = std::make_shared<Gate>();
            auto oldPort = std::make_shared<FunctionalAnalyticsPort>();
            oldPort->seriesFunction = [gate](const AnalyticsRequest&, const std::stop_token&) {
                gate->started = true;
                while (!gate->release.load()) {
                    QThread::msleep(1);
                }
                return seriesResult(1, "OLD");
            };
            auto newPort = std::make_shared<FunctionalAnalyticsPort>();
            newPort->seriesFunction = [](const AnalyticsRequest&, const std::stop_token&) {
                return seriesResult(22, "NEW");
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(oldPort));
            QSignalSpy invalidatedSpy(&model,
                                      &ssa::presentation::ActivityAnalyticsViewModel::invalidated);
            QSignalSpy succeededSpy(&model,
                                    &ssa::presentation::ActivityAnalyticsViewModel::succeeded);
            model.loadCustomSeries(testRequest("OLD"));
            QTRY_VERIFY_WITH_TIMEOUT(gate->started.load(), 1000);

            model.replaceService(serviceFor(newPort));
            QVERIFY(!model.loading());
            QVERIFY(model.customSeries().isEmpty());
            QCOMPARE(invalidatedSpy.count(), 1);
            model.loadCustomSeries(testRequest("NEW"));
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QCOMPARE(firstChartValue(model.customSeries()), 22.0);
            QCOMPARE(succeededSpy.count(), 1);

            gate->release = true;
            QTest::qWait(30);
            QCOMPARE(firstChartValue(model.customSeries()), 22.0);
            QCOMPARE(succeededSpy.count(), 1);
        }

        void importInvalidationDiscardsInFlightResult() {
            struct Gate final {
                std::atomic_bool started{false};
                std::atomic_bool release{false};
            };
            const auto gate = std::make_shared<Gate>();
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [gate](const AnalyticsRequest&, const std::stop_token&) {
                gate->started = true;
                while (!gate->release.load()) {
                    QThread::msleep(1);
                }
                return seriesResult(9);
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            QSignalSpy succeededSpy(&model,
                                    &ssa::presentation::ActivityAnalyticsViewModel::succeeded);
            model.loadCustomSeries(testRequest());
            QTRY_VERIFY_WITH_TIMEOUT(gate->started.load(), 1000);

            model.invalidateAfterImport();

            QVERIFY(!model.loading());
            QVERIFY(model.customSeries().isEmpty());
            gate->release = true;
            QTest::qWait(30);
            QVERIFY(model.customSeries().isEmpty());
            QCOMPARE(succeededSpy.count(), 0);
        }

        void importInvalidationNotifiesDashboardExactlyOnce() {
            auto port = std::make_shared<FunctionalAnalyticsPort>();
            port->seriesFunction = [](const AnalyticsRequest& request, const std::stop_token&) {
                return dashboardSeriesResult(request, 1);
            };
            ssa::presentation::ActivityAnalyticsViewModel model(serviceFor(port));
            model.loadDashboard(testPeriod(), testHistoryPeriod(), 30);
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 1000);
            QVERIFY(!model.dashboard().isEmpty());
            QSignalSpy changedSpy(&model,
                                  &ssa::presentation::ActivityAnalyticsViewModel::dashboardChanged);

            model.invalidateAfterImport();

            QCOMPARE(changedSpy.count(), 1);
            QVERIFY(model.dashboard().isEmpty());
        }
    };

} // namespace

QTEST_GUILESS_MAIN(ActivityAnalyticsViewModelTest)

#include "ActivityAnalyticsViewModelTest.moc"
