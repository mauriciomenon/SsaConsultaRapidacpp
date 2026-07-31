#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <array>
#include <memory>

namespace {

    struct QmlTypeRegistration final {
        const char* fileName;
        const char* typeName;
    };

    constexpr std::array<QmlTypeRegistration, 12> kAnalyticsTypes{{
        {"AnalyticsChart.qml", "AnalyticsChart"},
        {"AnalyticsChartCanvas.qml", "AnalyticsChartCanvas"},
        {"AnalyticsChartLegend.qml", "AnalyticsChartLegend"},
        {"AnalyticsChartTable.qml", "AnalyticsChartTable"},
        {"AnalyticsChartTooltip.qml", "AnalyticsChartTooltip"},
        {"SimpleBarChart.qml", "SimpleBarChart"},
        {"StackedBarChart.qml", "StackedBarChart"},
        {"PercentStackedBarChart.qml", "PercentStackedBarChart"},
        {"TrendLineChart.qml", "TrendLineChart"},
        {"AnalyticsChartCard.qml", "AnalyticsChartCard"},
        {"AnalyticsDashboard.qml", "AnalyticsDashboard"},
        {"AnalyticsCustomAnalysis.qml", "AnalyticsCustomAnalysis"},
    }};

    constexpr std::array<QmlTypeRegistration, 3> kControlTypes{{
        {"ActionButton.qml", "ActionButton"},
        {"AppComboBox.qml", "AppComboBox"},
        {"AppSpinBox.qml", "AppSpinBox"},
    }};

    constexpr std::array<const char*, 14> kDashboardKeys{{
        "registeredBySector",
        "registeredMonthly",
        "executedBySector",
        "executedMonthly",
        "partialAttentionBySector",
        "spgBySector",
        "apgBySector",
        "aplBySector",
        "pendingBySector",
        "pendingMonthly",
        "issuedByDivision",
        "issuedMonthly",
        "pendingDeadlinePercentage",
        "pendingDeadlineQuantity",
    }};

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    [[nodiscard]] QVariantMap chartModel(const QString& seriesName = QStringLiteral("total")) {
        return {
            {QStringLiteral("categories"), QStringList{QStringLiteral("SMIN")}},
            {QStringLiteral("series"),
             QVariantList{QVariantMap{{QStringLiteral("name"), seriesName},
                                      {QStringLiteral("values"), QVariantList{3.0}},
                                      {QStringLiteral("trendValues"), QVariantList{}}}}},
            {QStringLiteral("subtitle"), QStringLiteral("Semana observada: 2026-W26")},
            {QStringLiteral("qualityText"), QString{}},
            {QStringLiteral("available"), true},
        };
    }

    [[nodiscard]] QVariantMap deadlineModel() {
        return {
            {QStringLiteral("categories"), QStringList{QStringLiteral("2026-W26")}},
            {QStringLiteral("series"),
             QVariantList{
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("on_time")},
                             {QStringLiteral("values"), QVariantList{5.0}}},
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("warning")},
                             {QStringLiteral("values"), QVariantList{2.0}}},
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("overdue")},
                             {QStringLiteral("values"), QVariantList{1.0}}},
             }},
            {QStringLiteral("subtitle"), QStringLiteral("Semana observada: 2026-W26")},
            {QStringLiteral("qualityText"), QStringLiteral("3 itens fora do denominador")},
            {QStringLiteral("available"), true},
        };
    }

    [[nodiscard]] QVariantMap dashboardModel() {
        QVariantMap dashboard;
        for (const auto* key : kDashboardKeys) {
            dashboard.insert(QString::fromUtf8(key), chartModel());
        }
        dashboard.insert(QStringLiteral("executedBySector"),
                         chartModel(QStringLiteral("registered_in_period")));
        dashboard.insert(QStringLiteral("pendingDeadlinePercentage"), deadlineModel());
        dashboard.insert(QStringLiteral("pendingDeadlineQuantity"), deadlineModel());
        return dashboard;
    }

    class FakeAnalyticsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(QVariantMap dashboard READ dashboard NOTIFY dashboardChanged)
        Q_PROPERTY(QVariantMap customSeries READ customSeries NOTIFY customSeriesChanged)
        Q_PROPERTY(QVariantMap dimensionValues READ dimensionValues NOTIFY dimensionValuesChanged)
        Q_PROPERTY(
            QVariant warningWindowDays READ warningWindowDays NOTIFY warningWindowDaysChanged)

      public:
        explicit FakeAnalyticsViewModel(QObject* parent = nullptr)
            : QObject(parent), dashboard_(dashboardModel()), customSeries_(chartModel()),
              dimensionValues_({
                  {QStringLiteral("divisions"),
                   QStringList{QStringLiteral("SMI"), QStringLiteral("SMM")}},
                  {QStringLiteral("sectors"),
                   QStringList{QStringLiteral("SMIN"), QStringLiteral("SMIT")}},
                  {QStringLiteral("people"),
                   QStringList{QStringLiteral("Ana"), QStringLiteral("Bruno")}},
              }) {}

        [[nodiscard]] bool loading() const noexcept {
            return false;
        }

        [[nodiscard]] QString errorMessage() const {
            return errorMessage_;
        }

        [[nodiscard]] const QVariantMap& dashboard() const noexcept {
            return dashboard_;
        }

        [[nodiscard]] const QVariantMap& customSeries() const noexcept {
            return customSeries_;
        }

        [[nodiscard]] const QVariantMap& dimensionValues() const noexcept {
            return dimensionValues_;
        }

        [[nodiscard]] const QVariant& warningWindowDays() const noexcept {
            return warningWindowDays_;
        }

        Q_INVOKABLE bool requestDashboard(const QVariantMap& selection) {
            ++dashboardRequestCount_;
            dashboardRequestedDuringTerminalSignal_ =
                dashboardRequestedDuringTerminalSignal_ || terminalSignalActive_;
            callOrder_.push_back(QStringLiteral("dashboard"));
            lastDashboardSelection_ = selection;
            return true;
        }

        Q_INVOKABLE QVariantMap currentMonthSelection() const {
            return {{QStringLiteral("firstYear"), 2026},
                    {QStringLiteral("firstWeek"), 27},
                    {QStringLiteral("lastYear"), 2026},
                    {QStringLiteral("lastWeek"), 31}};
        }

        Q_INVOKABLE bool requestCustomSeries(const QVariantMap& selection) {
            ++customRequestCount_;
            lastCustomSelection_ = selection;
            return true;
        }

        Q_INVOKABLE bool requestDimensionValues(const QVariantMap& selection) {
            ++dimensionRequestCount_;
            callOrder_.push_back(QStringLiteral("dimensions"));
            lastDimensionSelection_ = selection;
            return true;
        }

        Q_INVOKABLE void loadWarningWindowDays() {
            ++warningLoadCount_;
            callOrder_.push_back(QStringLiteral("warning-load"));
        }

        Q_INVOKABLE bool saveWarningWindowDays(const int days) {
            ++warningSaveCount_;
            warningWindowDays_ = days;
            terminalSignalActive_ = true;
            emit warningWindowDaysChanged();
            terminalSignalActive_ = false;
            return true;
        }

        Q_INVOKABLE void cancel() {
            ++cancelCount_;
        }

        void completeWarningLoad(const int days) {
            warningWindowDays_ = days;
            terminalSignalActive_ = true;
            emit warningWindowDaysChanged();
            emit succeeded(4);
            emit warningWindowLoadFinished(true);
            terminalSignalActive_ = false;
        }

        void completeWarningLoadWithoutValue() {
            terminalSignalActive_ = true;
            emit succeeded(4);
            emit warningWindowLoadFinished(true);
            terminalSignalActive_ = false;
        }

        void failWarningLoad(const QString& message) {
            errorMessage_ = message;
            emit errorMessageChanged();
            terminalSignalActive_ = true;
            emit failed(message);
            emit warningWindowLoadFinished(false);
            terminalSignalActive_ = false;
        }

        [[nodiscard]] int dashboardRequestCount() const noexcept {
            return dashboardRequestCount_;
        }

        [[nodiscard]] int customRequestCount() const noexcept {
            return customRequestCount_;
        }

        [[nodiscard]] int dimensionRequestCount() const noexcept {
            return dimensionRequestCount_;
        }

        [[nodiscard]] int warningLoadCount() const noexcept {
            return warningLoadCount_;
        }

        [[nodiscard]] int warningSaveCount() const noexcept {
            return warningSaveCount_;
        }

        [[nodiscard]] int cancelCount() const noexcept {
            return cancelCount_;
        }

        [[nodiscard]] const QVariantMap& lastCustomSelection() const noexcept {
            return lastCustomSelection_;
        }

        [[nodiscard]] const QVariantMap& lastDashboardSelection() const noexcept {
            return lastDashboardSelection_;
        }

        [[nodiscard]] const QVariantMap& lastDimensionSelection() const noexcept {
            return lastDimensionSelection_;
        }

        [[nodiscard]] const QStringList& callOrder() const noexcept {
            return callOrder_;
        }

        [[nodiscard]] bool dashboardRequestedDuringTerminalSignal() const noexcept {
            return dashboardRequestedDuringTerminalSignal_;
        }

      signals:
        void stateChanged();
        void errorMessageChanged();
        void dashboardChanged();
        void customSeriesChanged();
        void dimensionValuesChanged();
        void warningWindowDaysChanged();
        void warningWindowLoadFinished(bool successful);
        // Generated by moc.
        // NOLINTNEXTLINE(clang-diagnostic-undefined-internal,readability-inconsistent-declaration-parameter-name)
        void succeeded(int kind);
        // Generated by moc.
        // NOLINTNEXTLINE(clang-diagnostic-undefined-internal,readability-inconsistent-declaration-parameter-name)
        void failed(QString message);

      private:
        QVariantMap dashboard_;
        QVariantMap customSeries_;
        QVariantMap dimensionValues_;
        QVariant warningWindowDays_;
        QString errorMessage_;
        QVariantMap lastDashboardSelection_;
        QVariantMap lastCustomSelection_;
        QVariantMap lastDimensionSelection_;
        QStringList callOrder_;
        int dashboardRequestCount_{0};
        int customRequestCount_{0};
        int dimensionRequestCount_{0};
        int warningLoadCount_{0};
        int warningSaveCount_{0};
        int cancelCount_{0};
        bool terminalSignalActive_{false};
        bool dashboardRequestedDuringTerminalSignal_{false};
    };

    [[nodiscard]] std::unique_ptr<QObject>
    loadWindow(QQmlEngine& engine, FakeAnalyticsViewModel& viewModel, QString& error) {
        const QDir analytics(
            repositoryRoot().filePath(QStringLiteral("app/desktop/qml/analytics")));
        QQmlComponent component(&engine, QUrl::fromLocalFile(analytics.filePath(
                                             QStringLiteral("AnalyticsWindow.qml"))));
        if (!component.isReady()) {
            error = component.errorString();
            return nullptr;
        }
        auto window = std::unique_ptr<QObject>(component.createWithInitialProperties(
            {{QStringLiteral("analyticsViewModel"), QVariant::fromValue(&viewModel)}}));
        if (!window) {
            error = component.errorString();
        }
        return window;
    }

    [[nodiscard]] bool invoke(QObject& object, const char* method) {
        return QMetaObject::invokeMethod(&object, method, Qt::DirectConnection);
    }

    [[nodiscard]] bool invoke(QObject& object, const char* method, const QVariant& argument) {
        return QMetaObject::invokeMethod(&object, method, Qt::DirectConnection,
                                         Q_ARG(QVariant, argument));
    }

    [[nodiscard]] bool waitForRenderedFrames(QQuickWindow& window) {
        QSignalSpy frameSpy(&window, &QQuickWindow::frameSwapped);
        for (int frame = 0; frame < 2; ++frame) {
            window.requestUpdate();
            if (frameSpy.isEmpty() && !frameSpy.wait(1000)) {
                return false;
            }
            frameSpy.clear();
        }
        return true;
    }

    [[nodiscard]] QString readSource(const QString& fileName) {
        QFile source(
            repositoryRoot().filePath(QStringLiteral("app/desktop/qml/analytics/") + fileName));
        if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(source.readAll());
    }

    class ActivityAnalyticsWindowQmlTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            const QDir root = repositoryRoot();
            const QUrl themeUrl =
                QUrl::fromLocalFile(root.filePath(QStringLiteral("app/desktop/qml/Theme.qml")));
            QVERIFY(qmlRegisterSingletonType(themeUrl, "SsaConsultaRapida", 1, 0, "Theme") >= 0);

            const QDir analytics(root.filePath(QStringLiteral("app/desktop/qml/analytics")));
            for (const auto& registration : kAnalyticsTypes) {
                const QUrl url = QUrl::fromLocalFile(
                    analytics.filePath(QString::fromUtf8(registration.fileName)));
                QVERIFY2(qmlRegisterType(url, "SsaConsultaRapida", 1, 0, registration.typeName) >=
                             0,
                         registration.typeName);
            }
            const QDir components(root.filePath(QStringLiteral("app/desktop/qml/components")));
            for (const auto& registration : kControlTypes) {
                const QUrl url = QUrl::fromLocalFile(
                    components.filePath(QString::fromUtf8(registration.fileName)));
                QVERIFY2(qmlRegisterType(url, "SsaConsultaRapida", 1, 0, registration.typeName) >=
                             0,
                         registration.typeName);
            }
        }

        void window_opens_closes_and_reopens_with_fourteen_chart_cards() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            QVERIFY(dashboard != nullptr);

            QCOMPARE(dashboard->property("chartCount").toInt(), 14);
            QCOMPARE(viewModel.warningLoadCount(), 1);
            QCOMPARE(viewModel.dashboardRequestCount(), 0);
            QCOMPARE(viewModel.dimensionRequestCount(), 0);

            QVERIFY(invoke(*object, "open"));
            QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
            window->close();
            QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
            QVERIFY(invoke(*object, "open"));
            QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
            window->close();
            QTRY_VERIFY_WITH_TIMEOUT(viewModel.cancelCount() >= 1, 1000);
        }

        void startup_serializes_latest_wins_requests_and_avoids_iso_week_53_default() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(dashboard != nullptr);
            QVERIFY(custom != nullptr);

            QCOMPARE(viewModel.callOrder(), QStringList{QStringLiteral("warning-load")});
            QVERIFY(dashboard->setProperty("reportFirstYear", 2021));
            QVERIFY(dashboard->setProperty("reportLastYear", 2021));
            QVERIFY(custom->setProperty("firstYear", 2021));
            QVERIFY(custom->setProperty("lastYear", 2021));

            viewModel.completeWarningLoadWithoutValue();
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);
            const QVariantMap dashboardSelection = viewModel.lastDashboardSelection();
            QCOMPARE(dashboardSelection.value(QStringLiteral("reportLastYear")).toInt(), 2021);
            QVERIFY(dashboardSelection.value(QStringLiteral("reportLastWeek")).toInt() <= 52);
            QVERIFY(!dashboardSelection.contains(QStringLiteral("warningWindowDays")));
            QVERIFY(!viewModel.dashboardRequestedDuringTerminalSignal());
            QCOMPARE(viewModel.dimensionRequestCount(), 0);

            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);
            const QVariantMap dimensionSelection = viewModel.lastDimensionSelection();
            QCOMPARE(dimensionSelection.value(QStringLiteral("lastYear")).toInt(), 2021);
            QVERIFY(dimensionSelection.value(QStringLiteral("lastWeek")).toInt() <= 52);
            const QStringList expectedCallOrder{QStringLiteral("warning-load"),
                                                QStringLiteral("dashboard"),
                                                QStringLiteral("dimensions")};
            QCOMPARE(viewModel.callOrder(), expectedCallOrder);

            QVERIFY(invoke(*object, "selectTab", 0));
            QVERIFY(invoke(*object, "selectTab", 1));
            QCOMPARE(viewModel.dimensionRequestCount(), 1);
        }

        void period_controls_default_to_current_month_and_allow_direct_week_entry() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            auto* dashboardFirstWeek =
                object->findChild<QObject*>(QStringLiteral("analyticsDashboardFirstWeek"));
            auto* dashboardLastWeek =
                object->findChild<QObject*>(QStringLiteral("analyticsDashboardLastWeek"));
            auto* customFirstWeek =
                object->findChild<QObject*>(QStringLiteral("analyticsCustomFirstWeek"));
            auto* customLastWeek =
                object->findChild<QObject*>(QStringLiteral("analyticsCustomLastWeek"));

            QVERIFY(dashboard != nullptr);
            QVERIFY(custom != nullptr);
            QVERIFY(dashboardFirstWeek != nullptr);
            QVERIFY(dashboardLastWeek != nullptr);
            QVERIFY(customFirstWeek != nullptr);
            QVERIFY(customLastWeek != nullptr);
            QCOMPARE(dashboard->property("reportFirstWeek").toInt(), 27);
            QCOMPARE(dashboard->property("reportLastWeek").toInt(), 31);
            QCOMPARE(custom->property("firstWeek").toInt(), 27);
            QCOMPARE(custom->property("lastWeek").toInt(), 31);
            QVERIFY(dashboardFirstWeek->property("editable").toBool());
            QVERIFY(dashboardLastWeek->property("editable").toBool());
            QVERIFY(customFirstWeek->property("editable").toBool());
            QVERIFY(customLastWeek->property("editable").toBool());
        }

        void dashboard_renders_both_reference_sizes_and_switches_to_one_column_when_narrow() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* firstYearInput =
                object->findChild<QObject*>(QStringLiteral("analyticsDashboardFirstYear"));
            auto* refreshButton =
                object->findChild<QObject*>(QStringLiteral("analyticsDashboardRefresh"));
            QVERIFY(window != nullptr);
            QVERIFY(dashboard != nullptr);
            QVERIFY(firstYearInput != nullptr);
            QVERIFY(refreshButton != nullptr);

            window->setGeometry(0, 0, 1180, 760);
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("columnCount").toInt(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("controlColumnCount").toInt(), 6, 1000);
            QVERIFY(firstYearInput->property("width").toReal() >= 80);
            QCOMPARE(firstYearInput->property("displayText").toString(), QStringLiteral("2026"));
            QVERIFY(refreshButton->property("width").toReal() >= 120);
            QTRY_VERIFY_WITH_TIMEOUT(!window->grabWindow().isNull(), 1000);
            const QImage compactImage = window->grabWindow();
            QVERIFY(compactImage.save(
                QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("activity-analytics-window-1180x760.png"))));

            window->setGeometry(0, 0, 1580, 940);
            QVERIFY(waitForRenderedFrames(*window));
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("columnCount").toInt(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("controlColumnCount").toInt(), 12, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!window->grabWindow().isNull(), 1000);
            const QImage wideImage = window->grabWindow();
            QVERIFY(wideImage.save(
                QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("activity-analytics-window-1580x940.png"))));

            window->setWidth(820);
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("columnCount").toInt(), 1, 1000);
            QVERIFY(invoke(*dashboard, "scrollToBottom"));
            QTRY_VERIFY_WITH_TIMEOUT(dashboard->property("scrollPosition").toReal() > 0, 1000);
        }

        void warning_window_stays_empty_until_loaded_and_persists_explicit_value() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            QVERIFY(dashboard != nullptr);

            QCOMPARE(dashboard->property("warningText").toString(), QString{});
            QVERIFY(invoke(*dashboard, "saveWarning"));
            QCOMPARE(viewModel.warningSaveCount(), 0);

            viewModel.completeWarningLoad(14);
            QTRY_COMPARE_WITH_TIMEOUT(dashboard->property("warningText").toString(),
                                      QStringLiteral("14"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);
            QVERIFY(invoke(*dashboard, "setWarningText", QStringLiteral("21")));
            QVERIFY(invoke(*dashboard, "saveWarning"));
            QCOMPARE(viewModel.warningSaveCount(), 1);
            QCOMPARE(viewModel.warningWindowDays().toInt(), 21);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 2, 1000);
            QVERIFY(!viewModel.dashboardRequestedDuringTerminalSignal());
        }

        void warning_load_failure_is_visible_and_explicit_save_recovers_dashboard() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* errorLabel = object->findChild<QObject*>(QStringLiteral("analyticsErrorMessage"));
            QVERIFY(dashboard != nullptr);
            QVERIFY(errorLabel != nullptr);

            viewModel.failWarningLoad(QStringLiteral("Falha ao ler configuracao de alerta"));
            QTRY_VERIFY_WITH_TIMEOUT(errorLabel->property("visible").toBool(), 1000);
            QCOMPARE(errorLabel->property("text").toString(),
                     QStringLiteral("Falha ao ler configuracao de alerta"));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);
            QVERIFY(!viewModel.dashboardRequestedDuringTerminalSignal());

            QVERIFY(invoke(*dashboard, "setWarningText", QStringLiteral("7")));
            QVERIFY(invoke(*dashboard, "saveWarning"));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 2, 1000);
            QVERIFY(!viewModel.dashboardRequestedDuringTerminalSignal());
        }

        void custom_analysis_requires_explicit_person_and_preserves_executor_default() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QCOMPARE(custom->property("personRoleIndex").toInt(), 2);
            QVERIFY(invoke(*custom, "setBreakdownIndex", 2));
            QCOMPARE(custom->property("requiresExplicitPeople").toBool(), true);
            QCOMPARE(custom->property("canAnalyze").toBool(), false);

            QVERIFY(invoke(*custom, "selectDivision", QStringLiteral("SMI")));
            QVERIFY(invoke(*custom, "selectSector", QStringLiteral("SMIN")));
            QVERIFY(invoke(*custom, "selectPerson", QStringLiteral("Ana")));
            QCOMPARE(custom->property("canAnalyze").toBool(), true);
            QVERIFY(invoke(*custom, "runAnalysis"));
            QCOMPARE(viewModel.customRequestCount(), 1);
            QCOMPARE(viewModel.lastCustomSelection().value(QStringLiteral("people")).toStringList(),
                     QStringList{QStringLiteral("Ana")});
            QVERIFY(viewModel.dimensionRequestCount() >= 2);
            QCOMPARE(viewModel.lastDimensionSelection()
                         .value(QStringLiteral("divisions"))
                         .toStringList(),
                     QStringList{QStringLiteral("SMI")});
        }

        void custom_analysis_drops_selections_invalidated_by_metric_and_person_role() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*custom, "selectDivision", QStringLiteral("SMI")));
            QVERIFY(invoke(*custom, "selectSector", QStringLiteral("SMIN")));
            QVERIFY(invoke(*custom, "selectPerson", QStringLiteral("Ana")));
            QVERIFY(invoke(*custom, "setPersonRoleIndex", 0));
            QCOMPARE(custom->property("selectedDivisions").toStringList(),
                     QStringList{QStringLiteral("SMI")});
            QCOMPARE(custom->property("selectedSectors").toStringList(),
                     QStringList{QStringLiteral("SMIN")});
            QVERIFY(custom->property("selectedPeople").toStringList().isEmpty());

            QVERIFY(invoke(*custom, "selectPerson", QStringLiteral("Bruno")));
            QVERIFY(invoke(*custom, "setMetricIndex", 1));
            QVERIFY(custom->property("selectedDivisions").toStringList().isEmpty());
            QVERIFY(custom->property("selectedSectors").toStringList().isEmpty());
            QVERIFY(custom->property("selectedPeople").toStringList().isEmpty());
        }

        void custom_chart_uses_bars_for_periods_and_stacks_deadline_classes() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            auto* chart = object->findChild<QObject*>(QStringLiteral("customAnalysisChart"));
            QVERIFY(custom != nullptr);
            QVERIFY(chart != nullptr);

            QCOMPARE(chart->property("chartType").toString(), QStringLiteral("bar"));
            QVERIFY(invoke(*custom, "setGrainIndex", 1));
            QTRY_COMPARE_WITH_TIMEOUT(chart->property("chartType").toString(),
                                      QStringLiteral("bar"), 1000);
            QVERIFY(invoke(*custom, "setGrainIndex", 2));
            QTRY_COMPARE_WITH_TIMEOUT(chart->property("chartType").toString(),
                                      QStringLiteral("bar"), 1000);
            QVERIFY(invoke(*custom, "setMetricIndex", 8));
            QTRY_COMPARE_WITH_TIMEOUT(chart->property("chartType").toString(),
                                      QStringLiteral("stackedBar"), 1000);
        }

        void chart_card_localizes_composite_quality_and_known_unavailability_reasons() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* chart = object->findChild<QObject*>(QStringLiteral("customAnalysisChart"));
            QVERIFY(chart != nullptr);

            auto qualityModel = chartModel();
            qualityModel.insert(QStringLiteral("qualityText"),
                                QStringLiteral("excluded_for_data_quality=3 | "
                                               "snapshot_stale_by_weeks=2"));
            QVERIFY(chart->setProperty("chartModel", qualityModel));
            QTRY_COMPARE_WITH_TIMEOUT(
                chart->property("qualityText").toString(),
                QStringLiteral("Itens fora do denominador: 3 | Captura atrasada em 2 semana(s)"),
                1000);

            auto unavailableModel = chartModel();
            unavailableModel.insert(QStringLiteral("available"), false);
            unavailableModel.insert(
                QStringLiteral("unavailableReason"),
                QStringLiteral("complete partial-attention source is unavailable"));
            QVERIFY(chart->setProperty("chartModel", unavailableModel));
            QTRY_COMPARE_WITH_TIMEOUT(
                chart->property("emptyMessage").toString(),
                QStringLiteral("Indisponivel: fonte completa de atencao parcial nao disponivel"),
                1000);
        }

        void qml_layer_contains_no_sql_or_canonical_table_access() {
            const QRegularExpression selectStatement(QStringLiteral("\\bSELECT\\b"),
                                                     QRegularExpression::CaseInsensitiveOption);
            const QStringList files{
                QStringLiteral("AnalyticsChartCard.qml"),
                QStringLiteral("AnalyticsDashboard.qml"),
                QStringLiteral("AnalyticsCustomAnalysis.qml"),
                QStringLiteral("AnalyticsWindow.qml"),
            };
            for (const auto& file : files) {
                const QString source = readSource(file);
                QVERIFY2(!source.isEmpty(), qPrintable(file));
                QVERIFY2(!selectStatement.match(source).hasMatch(), qPrintable(file));
                QVERIFY2(!source.contains(QStringLiteral("sqlite"), Qt::CaseInsensitive),
                         qPrintable(file));
                QVERIFY2(!source.contains(QStringLiteral("ssa_table"), Qt::CaseInsensitive),
                         qPrintable(file));
            }
        }
    };

} // namespace

QTEST_MAIN(ActivityAnalyticsWindowQmlTest)

#include "ActivityAnalyticsWindowQmlTest.moc"
