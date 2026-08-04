#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QBuffer>
#include <QEventLoop>
#include <QImage>
#include <QQuickItemGrabResult>
#include <QTimer>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <algorithm>
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

    constexpr std::array<QmlTypeRegistration, 5> kControlTypes{{
        {"ActionButton.qml", "ActionButton"},
        {"AppCheckBox.qml", "AppCheckBox"},
        {"AppComboBox.qml", "AppComboBox"},
        {"AppSpinBox.qml", "AppSpinBox"},
        {"AppTextField.qml", "AppTextField"},
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

    [[nodiscard]] QQuickItem* findVisualChild(QQuickItem& root, const QString& objectName) {
        if (root.objectName() == objectName) {
            return &root;
        }
        for (auto* child : root.childItems()) {
            if (auto* found = findVisualChild(*child, objectName)) {
                return found;
            }
        }
        return nullptr;
    }

    [[nodiscard]] QVariantMap chartModel(const QString& seriesName = QStringLiteral("total")) {
        return {
            {QStringLiteral("categories"), QStringList{QStringLiteral("SMIN")}},
            {QStringLiteral("series"),
             QVariantList{QVariantMap{{QStringLiteral("name"), seriesName},
                                      {QStringLiteral("values"), QVariantList{3.0}},
                                      {QStringLiteral("trendValues"), QVariantList{}}}}},
            {QStringLiteral("subtitle"), QStringLiteral("Semana observada: 202626")},
            {QStringLiteral("qualityText"), QString{}},
            {QStringLiteral("available"), true},
        };
    }

    [[nodiscard]] QVariantMap deadlineModel() {
        return {
            {QStringLiteral("categories"), QStringList{QStringLiteral("202626")}},
            {QStringLiteral("series"),
             QVariantList{
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("on_time")},
                             {QStringLiteral("values"), QVariantList{5.0}}},
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("warning")},
                             {QStringLiteral("values"), QVariantList{2.0}}},
                 QVariantMap{{QStringLiteral("name"), QStringLiteral("overdue")},
                             {QStringLiteral("values"), QVariantList{1.0}}},
             }},
            {QStringLiteral("subtitle"), QStringLiteral("Semana observada: 202626")},
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
                   QStringList{QStringLiteral("SMI"), QStringLiteral("SMM"),
                               QStringLiteral("IEE")}},
                  {QStringLiteral("sectors"),
                   QStringList{QStringLiteral("SMIN"), QStringLiteral("SMIT"),
                               QStringLiteral("IEE1")}},
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
            return {{QStringLiteral("year"), 2026},      {QStringLiteral("month"), 7},
                    {QStringLiteral("firstYear"), 2026}, {QStringLiteral("firstWeek"), 27},
                    {QStringLiteral("lastYear"), 2026},  {QStringLiteral("lastWeek"), 31}};
        }

        Q_INVOKABLE QVariantMap calendarMonthSelection(const int year, const int month) const {
            if (year == 2026 && month == 6) {
                return {{QStringLiteral("year"), 2026},      {QStringLiteral("month"), 6},
                        {QStringLiteral("firstYear"), 2026}, {QStringLiteral("firstWeek"), 23},
                        {QStringLiteral("lastYear"), 2026},  {QStringLiteral("lastWeek"), 27}};
            }
            if (year == 2027 && month == 1) {
                return {{QStringLiteral("year"), 2027},      {QStringLiteral("month"), 1},
                        {QStringLiteral("firstYear"), 2027}, {QStringLiteral("firstWeek"), 1},
                        {QStringLiteral("lastYear"), 2027},  {QStringLiteral("lastWeek"), 5}};
            }
            return currentMonthSelection();
        }

        Q_INVOKABLE QVariantMap yearToDateSelection() const {
            const QDate today = QDate::currentDate();
            int isoYear = 0;
            const int isoWeek = today.weekNumber(&isoYear);
            return {
                {QStringLiteral("year"), isoYear}, {QStringLiteral("month"), 0},
                {QStringLiteral("firstYear"), isoYear}, {QStringLiteral("firstWeek"), 1},
                {QStringLiteral("lastYear"), isoYear},  {QStringLiteral("lastWeek"), isoWeek}};
        }

        Q_INVOKABLE QVariantMap currentIsoWeekSelection() const {
            const QDate today = QDate::currentDate();
            int isoYear = 0;
            const int isoWeek = today.weekNumber(&isoYear);
            return {{QStringLiteral("year"), today.year()},
                    {QStringLiteral("month"), today.month()},
                    {QStringLiteral("firstYear"), isoYear},
                    {QStringLiteral("firstWeek"), isoWeek},
                    {QStringLiteral("lastYear"), isoYear},
                    {QStringLiteral("lastWeek"), isoWeek}};
        }

        Q_INVOKABLE QVariantMap currentIsoMonthSelection() const {
            const QDate month = QDate::currentDate().addMonths(-1);
            return {{QStringLiteral("year"), month.year()},
                    {QStringLiteral("month"), month.month()},
                    {QStringLiteral("firstYear"), month.year()},
                    {QStringLiteral("firstWeek"), 1},
                    {QStringLiteral("lastYear"), month.year()},
                    {QStringLiteral("lastWeek"), 4}};
        }

        Q_INVOKABLE void clearCustomSeries() {
            if (customSeries_.isEmpty()) {
                return;
            }
            customSeries_.clear();
            emit customSeriesChanged();
        }

        Q_INVOKABLE QString customChartTitle(const QVariantMap& selection) const {
            const int metric = selection.value(QStringLiteral("metric"), 0).toInt();
            const int breakdown = selection.value(QStringLiteral("breakdown"), 0).toInt();
            const int personRole = selection.value(QStringLiteral("personRole"), 2).toInt();
            static const QStringList metrics{QStringLiteral("SSAs cadastradas"),
                                             QStringLiteral("SSAs executadas")};
            static const QStringList breakdowns{
                QStringLiteral("Divisao"), QStringLiteral("Divisao e setor"),
                QStringLiteral("Divisao e pessoa"), QStringLiteral("Setor e pessoa")};
            static const QStringList roles{QStringLiteral("Solicitante"),
                                           QStringLiteral("Planejamento/programacao"),
                                           QStringLiteral("Execucao")};
            const QString firstWeek = QStringLiteral("%1%2")
                                          .arg(selection.value(QStringLiteral("firstYear")).toInt())
                                          .arg(selection.value(QStringLiteral("firstWeek")).toInt(),
                                               2, 10, QChar('0'));
            const QString lastWeek = QStringLiteral("%1%2")
                                         .arg(selection.value(QStringLiteral("lastYear")).toInt())
                                         .arg(selection.value(QStringLiteral("lastWeek")).toInt(),
                                              2, 10, QChar('0'));
            const QString period = firstWeek == lastWeek
                                       ? firstWeek
                                       : QStringLiteral("de %1 a %2").arg(firstWeek, lastWeek);
            return QStringLiteral("%1 %2 em %3 (%4)")
                .arg(metrics.value(metric), period, breakdowns.value(breakdown),
                     roles.value(personRole));
        }

        Q_INVOKABLE bool writeExportFile(const QString& path, const QString& content) const {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                return false;
            }
            return file.write(content.toUtf8()) >= 0;
        }

        void publishDimensionValues(const QVariantMap& values) {
            dimensionValues_ = values;
            emit dimensionValuesChanged();
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

        void makeDashboardChartUnavailable(const QString& key) {
            auto model = dashboard_.value(key).toMap();
            model.insert(QStringLiteral("available"), false);
            model.insert(QStringLiteral("unavailableReason"),
                         QStringLiteral("snapshot history is unavailable"));
            dashboard_.insert(key, model);
            emit dashboardChanged();
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

    [[nodiscard]] bool invoke(QObject& object, const char* method, const QVariant& first,
                              const QVariant& second) {
        return QMetaObject::invokeMethod(&object, method, Qt::DirectConnection,
                                         Q_ARG(QVariant, first), Q_ARG(QVariant, second));
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

    class ExportFileWriter final : public QObject {
        Q_OBJECT

      public:
        void setLastGrabbedItem(QQuickItem* item) { lastGrabbedItem_ = item; }
        [[nodiscard]] QQuickItem* lastGrabbedItem() const { return lastGrabbedItem_; }

        Q_INVOKABLE bool write(const QString& path, const QString& content) const {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                return false;
            }
            return file.write(content.toUtf8()) >= 0;
        }

        Q_INVOKABLE bool grabItemToFile(QObject* itemObject, const QString& path) const {
            auto* item = qobject_cast<QQuickItem*>(itemObject);
            if (item == nullptr || path.trimmed().isEmpty()) {
                return false;
            }
            const_cast<ExportFileWriter*>(this)->setLastGrabbedItem(item);
            const QSharedPointer<QQuickItemGrabResult> result = item->grabToImage();
            if (result.isNull()) {
                return false;
            }
            if (result->image().isNull()) {
                QEventLoop loop;
                QTimer timeout;
                timeout.setSingleShot(true);
                timeout.setInterval(5000);
                QObject::connect(result.data(), &QQuickItemGrabResult::ready, &loop,
                                 &QEventLoop::quit);
                QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                timeout.start();
                loop.exec();
            }
            if (result->image().isNull()) {
                return false;
            }
            return result->saveToFile(path);
        }

        Q_INVOKABLE bool grabItemToSvgFile(QObject* itemObject, const QString& path) const {
            auto* item = qobject_cast<QQuickItem*>(itemObject);
            if (item == nullptr || path.trimmed().isEmpty()) {
                return false;
            }
            const_cast<ExportFileWriter*>(this)->setLastGrabbedItem(item);
            const QSharedPointer<QQuickItemGrabResult> result = item->grabToImage();
            if (result.isNull()) {
                return false;
            }
            if (result->image().isNull()) {
                QEventLoop loop;
                QTimer timeout;
                timeout.setSingleShot(true);
                timeout.setInterval(5000);
                QObject::connect(result.data(), &QQuickItemGrabResult::ready, &loop,
                                 &QEventLoop::quit);
                QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                timeout.start();
                loop.exec();
            }
            const QImage image = result->image();
            if (image.isNull()) {
                return false;
            }
            QByteArray pngBytes;
            QBuffer buffer(&pngBytes);
            if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
                return false;
            }
            const QString svg =
                QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
                + QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" ")
                + QStringLiteral("xmlns:xlink=\"http://www.w3.org/1999/xlink\" ")
                + QStringLiteral("width=\"%1\" height=\"%2\">").arg(image.width()).arg(image.height())
                + QStringLiteral("<image width=\"%1\" height=\"%2\" xlink:href=\"data:image/png;base64,")
                      .arg(image.width())
                      .arg(image.height())
                + QString::fromLatin1(pngBytes.toBase64()) + QStringLiteral("\"/></svg>");
            return write(path, svg);
        }

      private:
        QQuickItem* lastGrabbedItem_ = nullptr;
    };

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

        void monthly_navigation_moves_to_previous_month_and_keeps_direct_iso_editing() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(dashboard != nullptr);
            QVERIFY(custom != nullptr);

            viewModel.completeWarningLoad(14);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);

            QVERIFY(invoke(*dashboard, "previousMonth"));
            QCOMPARE(dashboard->property("reportFirstWeek").toInt(), 23);
            QCOMPARE(dashboard->property("reportLastWeek").toInt(), 27);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 2, 1000);

            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);
            const int dimensionBaseline = viewModel.dimensionRequestCount();

            QVERIFY(invoke(*custom, "previousMonth"));
            QCOMPARE(custom->property("firstWeek").toInt(), 23);
            QCOMPARE(custom->property("lastWeek").toInt(), 27);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), dimensionBaseline + 1,
                                      1000);
            QVERIFY(viewModel.customSeries().isEmpty());
        }

        void h1_dashboard_previous_month_queues_dashboard_refresh() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            QVERIFY(dashboard != nullptr);

            viewModel.completeWarningLoad(14);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);
            const int baseline = viewModel.dashboardRequestCount();

            QVERIFY(invoke(*dashboard, "previousMonth"));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), baseline + 1, 1000);
        }

        void h1_dashboard_year_to_date_queues_full_selected_period() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            QVERIFY(dashboard != nullptr);

            viewModel.completeWarningLoad(14);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 1, 1000);
            const QVariantMap ytd = viewModel.yearToDateSelection();

            QVERIFY(invoke(*dashboard, "applyYearToDate"));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dashboardRequestCount(), 2, 1000);
            const QVariantMap selection = viewModel.lastDashboardSelection();
            QCOMPARE(selection.value(QStringLiteral("reportFirstYear")),
                     ytd.value(QStringLiteral("firstYear")));
            QCOMPARE(selection.value(QStringLiteral("reportFirstWeek")),
                     ytd.value(QStringLiteral("firstWeek")));
            QCOMPARE(selection.value(QStringLiteral("reportLastYear")),
                     ytd.value(QStringLiteral("lastYear")));
            QCOMPARE(selection.value(QStringLiteral("reportLastWeek")),
                     ytd.value(QStringLiteral("lastWeek")));
        }

        void h2_custom_first_week_edit_refreshes_dimensions() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);
            const int baseline = viewModel.dimensionRequestCount();

            QVERIFY(invoke(*custom, "notifyFirstWeekEdited", QVariant(20)));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), baseline + 1, 1000);
        }

        void h3_custom_period_change_clears_stale_chart_series() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*object, "selectTab", 1));
            QVERIFY(!viewModel.customSeries().isEmpty());

            QVERIFY(invoke(*custom, "notifyFirstWeekEdited", QVariant(20)));
            QTRY_VERIFY_WITH_TIMEOUT(viewModel.customSeries().isEmpty(), 1000);
        }

        void h4_select_all_divisions_selects_every_option() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);

            QVERIFY(invoke(*custom, "selectAllDivisions"));
            const auto divisions =
                viewModel.dimensionValues().value(QStringLiteral("divisions")).toStringList();
            QCOMPARE(custom->property("selectedDivisions").toStringList(), divisions);
            QCOMPARE(custom->property("selectedDivisions").toStringList().size(), divisions.size());
        }

        void h5_year_to_date_preset_arms_period_and_analysis_request() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            const QVariantMap ytd = viewModel.yearToDateSelection();
            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);

            QVERIFY(invoke(*custom, "applyYearToDate"));
            QCOMPARE(custom->property("firstWeek").toInt(), 1);
            QCOMPARE(custom->property("lastWeek").toInt(),
                     ytd.value(QStringLiteral("lastWeek")).toInt());

            QVERIFY(invoke(*custom, "runAnalysis"));
            QCOMPARE(viewModel.customRequestCount(), 1);
            QCOMPARE(viewModel.lastCustomSelection().value(QStringLiteral("firstWeek")).toInt(), 1);
            QCOMPARE(viewModel.lastCustomSelection().value(QStringLiteral("lastWeek")).toInt(),
                     ytd.value(QStringLiteral("lastWeek")).toInt());
        }

        void h6_dimension_refresh_prunes_removed_division_selections() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*object, "selectTab", 1));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), 1, 1000);
            QVERIFY(invoke(*custom, "selectDivision", QStringLiteral("SMI")));
            QCOMPARE(custom->property("selectedDivisions").toStringList(),
                     QStringList{QStringLiteral("SMI")});

            viewModel.publishDimensionValues(
                {{QStringLiteral("divisions"),
                  QStringList{QStringLiteral("SMM"), QStringLiteral("IEE")}},
                 {QStringLiteral("sectors"),
                  QStringList{QStringLiteral("SMIN"), QStringLiteral("SMIT"),
                              QStringLiteral("IEE1")}},
                 {QStringLiteral("people"),
                  QStringList{QStringLiteral("Ana"), QStringLiteral("Bruno")}}});
            QTRY_VERIFY_WITH_TIMEOUT(custom->property("selectedDivisions").toStringList().isEmpty(),
                                     1000);
        }

        void h7_custom_grain_change_clears_stale_chart_series() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*object, "selectTab", 1));
            QVERIFY(!viewModel.customSeries().isEmpty());

            QVERIFY(invoke(*custom, "runAnalysis"));
            QVERIFY(invoke(*custom, "setGrainIndex", QVariant(1)));
            QTRY_VERIFY_WITH_TIMEOUT(viewModel.customSeries().isEmpty(), 1000);
        }

        void monthly_navigation_rejects_months_after_last_complete_month() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* dashboard = object->findChild<QObject*>(QStringLiteral("analyticsDashboard"));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(dashboard != nullptr);
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*dashboard, "applyCalendarMonth", 2027, 1));
            QCOMPARE(dashboard->property("periodYear").toInt(), 2026);
            QCOMPARE(dashboard->property("periodMonth").toInt(), 7);
            QVERIFY(invoke(*custom, "applyCalendarMonth", 2027, 1));
            QCOMPARE(custom->property("periodYear").toInt(), 2026);
            QCOMPARE(custom->property("periodMonth").toInt(), 7);
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

        void custom_dimension_option_toggles_on_real_mouse_click() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            auto* option = findVisualChild(*window->contentItem(),
                                           QStringLiteral("analyticsDivisionOption-IEE"));
            QVERIFY(option != nullptr);
            QVERIFY(findVisualChild(*window->contentItem(),
                                    QStringLiteral("analyticsSectorOption-IEE1")) != nullptr);
            const QPoint clickPoint =
                option->mapToScene(QPointF(option->width() / 2, option->height() / 2)).toPoint();

            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPoint);

            QTRY_VERIFY_WITH_TIMEOUT(option->property("checked").toBool(), 1000);
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);
            QCOMPARE(custom->property("selectedDivisions").toStringList(),
                     QStringList{QStringLiteral("IEE")});
            QVERIFY(option->property("textContrast").toReal() >= 4.5);
            option->forceActiveFocus();
            QTRY_VERIFY_WITH_TIMEOUT(option->property("focusHighlighted").toBool(), 1000);
            if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
                QVERIFY(waitForRenderedFrames(*window));
                const QImage customImage = window->grabWindow();
                QVERIFY(!customImage.isNull());
                QVERIFY(customImage.save(
                    QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("activity-analytics-custom-iee-1180x760.png"))));
            }
        }

        void custom_action_buttons_fit_their_labels_at_reference_width() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1180, 760);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));

            const QStringList names{QStringLiteral("analyticsRefreshOptions"),
                                    QStringLiteral("analyticsOverdueByArea"),
                                    QStringLiteral("analyticsExecutedByPerson")};
            const QVariantMap minimumWidths{{QStringLiteral("analyticsRefreshOptions"), 160.0},
                                            {QStringLiteral("analyticsOverdueByArea"), 175.0},
                                            {QStringLiteral("analyticsExecutedByPerson"), 205.0}};
            for (const auto& name : names) {
                auto* button = findVisualChild(*window->contentItem(), name);
                QVERIFY2(button != nullptr, qPrintable(name));
                if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
                    QVERIFY2(button->width() >= minimumWidths.value(name).toReal(),
                             qPrintable(name));
                    continue;
                }
                const qreal requiredWidth = button->property("implicitContentWidth").toReal() +
                                            button->property("leftPadding").toReal() +
                                            button->property("rightPadding").toReal();
                QVERIFY2(button->width() >= requiredWidth, qPrintable(name));
            }
        }

        void executed_by_person_preset_configures_whole_period_sector_chart() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);

            QVERIFY(invoke(*custom, "configureExecutedByPerson"));

            QCOMPARE(custom->property("metricIndex").toInt(), 1);
            QCOMPARE(custom->property("grainIndex").toInt(), 0);
            QCOMPARE(custom->property("breakdownIndex").toInt(), 3);
            QCOMPARE(custom->property("personRoleIndex").toInt(), 2);
            QVERIFY(viewModel.dimensionRequestCount() >= 1);
        }

        void executed_by_person_preset_waits_for_current_dimension_values() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1180, 760);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            auto* button = findVisualChild(*window->contentItem(),
                                           QStringLiteral("analyticsExecutedByPerson"));
            QVERIFY(button != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(button->width() > 0 && button->height() > 0, 1000);
            auto* controls = findVisualChild(*window->contentItem(),
                                             QStringLiteral("analyticsCustomActionControls"));
            QVERIFY(controls != nullptr);
            const qreal buttonX = button->mapToItem(controls, QPointF{}).x();
            controls->setProperty(
                "contentX", std::max<qreal>(0, buttonX + button->width() - controls->width()));
            QTRY_VERIFY_WITH_TIMEOUT(button->mapToItem(controls, QPointF{}).x() >= 0 &&
                                         button->mapToItem(controls, QPointF{}).x() +
                                                 button->width() <=
                                             controls->width(),
                                     1000);

            const int requestsBefore = viewModel.customRequestCount();
            const int dimensionsBefore = viewModel.dimensionRequestCount();
            QVERIFY(QMetaObject::invokeMethod(button, "click", Qt::DirectConnection));

            QTRY_COMPARE_WITH_TIMEOUT(viewModel.dimensionRequestCount(), dimensionsBefore + 1,
                                      1000);
            QCOMPARE(viewModel.customRequestCount(), requestsBefore);
            viewModel.publishDimensionValues(
                {{QStringLiteral("divisions"), QStringList{QStringLiteral("SMM")}},
                 {QStringLiteral("sectors"), QStringList{QStringLiteral("SMM1")}},
                 {QStringLiteral("people"),
                  QStringList{QStringLiteral("Ana Atual"), QStringLiteral("Bruno Atual")}}});

            QTRY_COMPARE_WITH_TIMEOUT(viewModel.customRequestCount(), requestsBefore + 1, 1000);
            const QVariantMap selection = viewModel.lastCustomSelection();
            const QStringList expectedPeople{QStringLiteral("Ana Atual"),
                                             QStringLiteral("Bruno Atual")};
            QCOMPARE(selection.value(QStringLiteral("sectors")).toStringList(),
                     QStringList{QStringLiteral("SMM1")});
            QCOMPARE(selection.value(QStringLiteral("people")).toStringList(), expectedPeople);
        }

        void executed_by_person_preset_auto_runs_custom_analysis() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1180, 760);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            auto* button =
                findVisualChild(*window->contentItem(), QStringLiteral("analyticsOverdueByArea"));
            QVERIFY(button != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(button->width() > 0 && button->height() > 0, 1000);
            auto* controls = findVisualChild(*window->contentItem(),
                                             QStringLiteral("analyticsCustomActionControls"));
            QVERIFY(controls != nullptr);
            const qreal buttonX = button->mapToItem(controls, QPointF{}).x();
            controls->setProperty(
                "contentX", std::max<qreal>(0, buttonX + button->width() - controls->width()));
            QTRY_VERIFY_WITH_TIMEOUT(button->mapToItem(controls, QPointF{}).x() >= 0 &&
                                         button->mapToItem(controls, QPointF{}).x() +
                                                 button->width() <=
                                             controls->width(),
                                     1000);
            viewModel.completeWarningLoad(14);

            const int requestsBefore = viewModel.customRequestCount();
            QVERIFY(QMetaObject::invokeMethod(button, "click", Qt::DirectConnection));

            QTRY_VERIFY_WITH_TIMEOUT(viewModel.customRequestCount() > requestsBefore, 1000);
        }

        void custom_tab_defaults_to_executed_metric() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));

            QCOMPARE(custom->property("metricIndex").toInt(), 1);
        }

        void executed_by_sector_week_preset_auto_runs_with_current_iso_week() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1280, 760);
            auto* button = findVisualChild(*window->contentItem(),
                                           QStringLiteral("analyticsExecutedBySectorWeek"));
            QVERIFY(button != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));

            const int requestsBefore = viewModel.customRequestCount();
            const QPoint clickPoint =
                button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPoint);

            QTRY_VERIFY_WITH_TIMEOUT(viewModel.customRequestCount() > requestsBefore, 1000);
            const QVariantMap selection = viewModel.lastCustomSelection();
            QCOMPARE(selection.value(QStringLiteral("metric")).toInt(), 1);
            QCOMPARE(selection.value(QStringLiteral("breakdown")).toInt(), 1);
            const QVariantMap isoWeek = viewModel.currentIsoWeekSelection();
            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(),
                     isoWeek.value(QStringLiteral("firstYear")).toInt());
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(),
                     isoWeek.value(QStringLiteral("firstWeek")).toInt());
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(),
                     isoWeek.value(QStringLiteral("lastYear")).toInt());
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(),
                     isoWeek.value(QStringLiteral("lastWeek")).toInt());
        }

        void executed_by_sector_month_preset_auto_runs_with_current_iso_month() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1280, 760);
            auto* button = findVisualChild(*window->contentItem(),
                                           QStringLiteral("analyticsExecutedBySectorMonth"));
            QVERIFY(button != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            auto* controls = findVisualChild(*window->contentItem(),
                                             QStringLiteral("analyticsCustomActionControls"));
            QVERIFY(controls != nullptr);
            const qreal buttonX = button->mapToItem(controls, QPointF{}).x();
            controls->setProperty(
                "contentX", std::max<qreal>(0, buttonX + button->width() - controls->width()));
            QTRY_VERIFY_WITH_TIMEOUT(button->mapToItem(controls, QPointF{}).x() >= 0 &&
                                         button->mapToItem(controls, QPointF{}).x() +
                                                 button->width() <=
                                             controls->width(),
                                     1000);

            const int requestsBefore = viewModel.customRequestCount();
            const QPoint clickPoint =
                button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPoint);

            QTRY_VERIFY_WITH_TIMEOUT(viewModel.customRequestCount() > requestsBefore, 1000);
            const QVariantMap selection = viewModel.lastCustomSelection();
            QCOMPARE(selection.value(QStringLiteral("metric")).toInt(), 1);
            QCOMPARE(selection.value(QStringLiteral("breakdown")).toInt(), 1);
            const QVariantMap isoMonth = viewModel.currentIsoMonthSelection();
            QCOMPARE(selection.value(QStringLiteral("firstYear")).toInt(),
                     isoMonth.value(QStringLiteral("firstYear")).toInt());
            QCOMPARE(selection.value(QStringLiteral("firstWeek")).toInt(),
                     isoMonth.value(QStringLiteral("firstWeek")).toInt());
            QCOMPARE(selection.value(QStringLiteral("lastYear")).toInt(),
                     isoMonth.value(QStringLiteral("lastYear")).toInt());
            QCOMPARE(selection.value(QStringLiteral("lastWeek")).toInt(),
                     isoMonth.value(QStringLiteral("lastWeek")).toInt());
        }

        void custom_action_controls_keep_all_buttons_reachable_at_minimum_width() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 760, 760);
            QVERIFY(invoke(*object, "selectTab", 1));
            window->show();
            QVERIFY(waitForRenderedFrames(*window));
            auto* controls = findVisualChild(*window->contentItem(),
                                             QStringLiteral("analyticsCustomActionControls"));
            auto* button = findVisualChild(*window->contentItem(),
                                           QStringLiteral("analyticsExecutedBySectorPerson"));
            QVERIFY(controls != nullptr);
            QVERIFY(button != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(
                controls->property("contentWidth").toReal() > controls->width(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(controls->height() > button->height(), 1000);

            const qreal buttonX = button->mapToItem(controls, QPointF{}).x();
            controls->setProperty(
                "contentX", std::max<qreal>(0, buttonX + button->width() - controls->width()));
            QTRY_VERIFY_WITH_TIMEOUT(button->mapToItem(controls, QPointF{}).x() >= 0 &&
                                         button->mapToItem(controls, QPointF{}).x() +
                                                 button->width() <=
                                             controls->width(),
                                     1000);
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

        void custom_chart_title_adapts_to_selection() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            auto* chart = object->findChild<QObject*>(QStringLiteral("customAnalysisChart"));
            QVERIFY(custom != nullptr);
            QVERIFY(chart != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));

            const QString title = chart->property("title").toString();
            QVERIFY2(!title.contains(QStringLiteral("Resultado da analise customizada")),
                     qPrintable(title));
            QVERIFY(title.contains(QStringLiteral("SSAs executadas")));
            QVERIFY(title.contains(QStringLiteral("Execucao")));
        }

        void executed_by_sector_person_preset_configures_and_auto_runs() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* custom = object->findChild<QObject*>(QStringLiteral("analyticsCustomAnalysis"));
            QVERIFY(custom != nullptr);
            QVERIFY(invoke(*object, "selectTab", 1));

            QVERIFY(invoke(*custom, "configureExecutedBySectorPerson"));
            QCOMPARE(custom->property("metricIndex").toInt(), 1);
            QCOMPARE(custom->property("breakdownIndex").toInt(), 3);
            QCOMPARE(custom->property("personRoleIndex").toInt(), 2);
            const QVariantMap isoWeek = viewModel.currentIsoWeekSelection();
            QCOMPARE(custom->property("firstWeek").toInt(),
                     isoWeek.value(QStringLiteral("firstWeek")).toInt());
            QCOMPARE(custom->property("lastWeek").toInt(),
                     isoWeek.value(QStringLiteral("lastWeek")).toInt());

            const int requestsBefore = viewModel.customRequestCount();
            QVERIFY(invoke(*custom, "selectAllSectors"));
            QVERIFY(invoke(*custom, "selectAllPeople"));
            QVERIFY(invoke(*custom, "runAnalysis"));
            QCOMPARE(viewModel.customRequestCount(), requestsBefore + 1);
            const QVariantMap selection = viewModel.lastCustomSelection();
            QCOMPARE(selection.value(QStringLiteral("breakdown")).toInt(), 3);
            QCOMPARE(selection.value(QStringLiteral("personRole")).toInt(), 2);
        }

        void custom_chart_exposes_export_actions_when_data_present() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->setGeometry(0, 0, 1280, 760);
            window->show();
            QVERIFY(waitForRenderedFrames(*window));

            auto* chart = object->findChild<QObject*>(QStringLiteral("customAnalysisChart"));
            QVERIFY(chart != nullptr);
            QVERIFY(chart->property("showExportActions").toBool());
            QVERIFY(findVisualChild(*window->contentItem(),
                                    QStringLiteral("analyticsChartExportPng")) != nullptr);
            QVERIFY(findVisualChild(*window->contentItem(),
                                    QStringLiteral("analyticsChartExportSvg")) != nullptr);
        }

        void analytics_chart_harness_writes_png_and_svg_files() {
            QQmlEngine engine;
            ExportFileWriter writer;
            engine.rootContext()->setContextProperty(QStringLiteral("exportFileWriter"), &writer);

            QQmlComponent component(&engine);
            component.setData(R"QML(
                import QtQuick
                import SsaConsultaRapida

                Item {
                    id: harnessRoot
                    width: 640
                    height: 360

                    SimpleBarChart {
                        id: chart
                        objectName: "chart"
                        anchors.fill: parent
                        title: "Export harness"
                        categories: ["SMIN", "SMIT"]
                        series: [
                            {
                                "name": "Joao Silva",
                                "tag": "JS",
                                "values": [2, 5]
                            },
                            {
                                "name": "Maria Costa",
                                "tag": "MC",
                                "values": [3, 1]
                            }
                        ]
                        fileWriter: (path, content) => exportFileWriter.write(path, content)
                        itemGrabber: (item, path) => exportFileWriter.grabItemToFile(item, path)
                        svgGrabber: (item, path) => exportFileWriter.grabItemToSvgFile(item, path)
                    }
                }
            )QML",
                              QUrl(QStringLiteral("inmemory:/AnalyticsChartExportHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> harness(component.create());
            QVERIFY2(harness != nullptr, qPrintable(component.errorString()));

            QQuickWindow window;
            window.setGeometry(0, 0, 640, 360);
            auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
            QVERIFY(harnessItem != nullptr);
            harnessItem->setParentItem(window.contentItem());
            window.show();
            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            QVERIFY(waitForRenderedFrames(window));

            auto* chart = harness->findChild<QQuickItem*>(QStringLiteral("chart"));
            QVERIFY(chart != nullptr);
            QVERIFY(chart->property("plotWidth").toReal() > 0);
            QVERIFY(chart->property("hasData").toBool());

            QTemporaryDir outputDirectory;
            QVERIFY(outputDirectory.isValid());
            const QString pngPath = outputDirectory.filePath(QStringLiteral("analytics-chart.png"));
            const QString svgPath = outputDirectory.filePath(QStringLiteral("analytics-chart.svg"));
            const QUrl pngUrl = QUrl::fromLocalFile(pngPath);
            const QUrl svgUrl = QUrl::fromLocalFile(svgPath);

            QSignalSpy exportSpy(chart, SIGNAL(exportFinished(bool)));

            auto* snapshot = chart->findChild<QQuickItem*>(QStringLiteral("analyticsChartSnapshot"));
            auto* canvas = chart->findChild<QQuickItem*>(QStringLiteral("analyticsChartCanvas"));
            QVERIFY(snapshot != nullptr);
            QVERIFY(canvas != nullptr);
            QVERIFY(snapshot->height() > canvas->height());

            writer.setLastGrabbedItem(nullptr);
            QVERIFY(QMetaObject::invokeMethod(chart, "savePng", Q_ARG(QVariant, QVariant::fromValue(pngUrl))));
            QCOMPARE(exportSpy.count(), 1);
            QCOMPARE(exportSpy.at(0).at(0).toBool(), true);
            QCOMPARE(writer.lastGrabbedItem(), snapshot);
            const QImage pngImage(pngPath);
            QVERIFY(!pngImage.isNull());
            QVERIFY(pngImage.width() > 0);
            QVERIFY(pngImage.height() > 0);
            QVERIFY(pngImage.height() > static_cast<int>(canvas->height()));

            writer.setLastGrabbedItem(nullptr);
            QVERIFY(QMetaObject::invokeMethod(chart, "saveSvg", Q_ARG(QVariant, QVariant::fromValue(svgUrl))));
            QCOMPARE(exportSpy.count(), 2);
            QCOMPARE(exportSpy.at(1).at(0).toBool(), true);
            QCOMPARE(writer.lastGrabbedItem(), snapshot);
            QFile svgFile(svgPath);
            QVERIFY(svgFile.open(QIODevice::ReadOnly));
            const QByteArray svgPayload = svgFile.readAll();
            QVERIFY(svgPayload.contains("<svg"));
            QVERIFY(svgPayload.contains("data:image/png;base64,"));
        }

        void custom_chart_export_writes_png_and_svg_files() {
            analytics_chart_harness_writes_png_and_svg_files();
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

        void dashboard_compacts_unavailable_chart_cards() {
            QQmlEngine engine;
            FakeAnalyticsViewModel viewModel;
            QString error;
            auto object = loadWindow(engine, viewModel, error);
            QVERIFY2(object != nullptr, qPrintable(error));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->show();
            QTRY_VERIFY_WITH_TIMEOUT(
                findVisualChild(*window->contentItem(), QStringLiteral("analyticsChartCard-0")) !=
                    nullptr,
                1000);
            auto* chart =
                findVisualChild(*window->contentItem(), QStringLiteral("analyticsChartCard-0"));

            QCOMPARE(chart->property("preferredCardHeight").toReal(), 380.0);
            viewModel.makeDashboardChartUnavailable(QStringLiteral("executedBySector"));
            QTRY_COMPARE_WITH_TIMEOUT(chart->property("preferredCardHeight").toReal(), 140.0, 1000);
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
