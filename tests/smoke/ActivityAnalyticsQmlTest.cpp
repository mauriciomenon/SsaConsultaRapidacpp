#include <QAccessible>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QJSValue>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QTimer>
#include <QtQml/qqml.h>

#include <array>
#include <memory>
#include <utility>

namespace {

    struct QmlTypeRegistration {
        const char* fileName;
        const char* typeName;
    };

    constexpr std::array<QmlTypeRegistration, 9> kAnalyticsTypes{{
        {"AnalyticsChart.qml", "AnalyticsChart"},
        {"AnalyticsChartCanvas.qml", "AnalyticsChartCanvas"},
        {"AnalyticsChartLegend.qml", "AnalyticsChartLegend"},
        {"AnalyticsChartTable.qml", "AnalyticsChartTable"},
        {"AnalyticsChartTooltip.qml", "AnalyticsChartTooltip"},
        {"SimpleBarChart.qml", "SimpleBarChart"},
        {"StackedBarChart.qml", "StackedBarChart"},
        {"PercentStackedBarChart.qml", "PercentStackedBarChart"},
        {"TrendLineChart.qml", "TrendLineChart"},
    }};

    constexpr auto kSimpleBarHarness = R"QML(
        import QtQuick
        import SsaConsultaRapida

        SimpleBarChart {
            objectName: "simpleChart"
            width: 640
            height: 420
            title: "Executadas"
            tableVisible: true
            categories: ["SEE", ""]
            series: [{
                name: "SSAs",
                values: [5, null]
            }]

            function mutateFirstValue() {
                series[0].values[0] = 9;
                refresh();
            }
        }
    )QML";

    constexpr auto kStackedBarHarness = R"QML(
        import QtQuick
        import SsaConsultaRapida

        Item {
            width: 640
            height: 420

            StackedBarChart {
                objectName: "stackedChart"
                anchors.fill: parent
                categories: ["SEE", "MEL"]
                series: [{
                    name: "Periodo",
                    values: [3, 1]
                }, {
                    name: "Anteriores",
                    values: [4, 2]
                }]
            }

            PercentStackedBarChart {
                objectName: "percentChart"
                anchors.fill: parent
                visible: false
                categories: ["2026-W01"]
                series: [{
                    name: "No prazo",
                    values: [25]
                }, {
                    name: "Alerta",
                    values: [75]
                }]
            }
        }
    )QML";

    constexpr auto kTrendLineHarness = R"QML(
        import QtQuick
        import SsaConsultaRapida

        TrendLineChart {
            objectName: "trendChart"
            width: 640
            height: 420
            categories: ["Jan", "Fev", "Mar"]
            series: [{
                name: "Executadas",
                values: [10, null, 30],
                trendValues: [12, 20, 28]
            }]
        }
    )QML";

    constexpr auto kLongCategoryHarness = R"QML(
        import QtQuick
        import SsaConsultaRapida

        AnalyticsChartCanvas {
            width: 640
            height: 420
            chartType: "bar"
            categories: [
                "IEE / IEE2 / ACOSTA FERNANDEZ RAMON ARIEL",
                "IEE / IEE2 / LUCAS COSTA CICARELLI",
                "IEE / IEE2 / MARCOS ALOE YAMAMOTO",
                "IEE / IEE3 / MAURICIO MENON",
                "IEE / IEE3 / SANCHEZ ALVARENGA BLAS CIRILO",
                "IEE / IEE4 / COSTA FERNANDEZ RAMON ARIEL"
            ]
            series: [{name: "Total", values: [1, 2, 3, 4, 5, 6]}]
        }
    )QML";

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    [[nodiscard]] std::unique_ptr<QObject> loadQml(QQmlEngine& engine, const char* source,
                                                   QString& error) {
        QStringList diagnostics;
        const QMetaObject::Connection warningConnection =
            QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                             [&diagnostics](const QList<QQmlError>& warnings) {
                                 for (const auto& warning : warnings) {
                                     diagnostics.push_back(warning.toString());
                                 }
                             });
        QQmlComponent component(&engine);
        component.setData(source, QUrl(QStringLiteral("inmemory:/ActivityAnalyticsHarness.qml")));
        if (component.isLoading()) {
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&component, &QQmlComponent::statusChanged, &loop,
                             [&component, &loop](QQmlComponent::Status status) {
                                 if (status != QQmlComponent::Loading) {
                                     loop.quit();
                                 }
                             });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            timeout.start(1000);
            loop.exec();
        }
        if (!component.isReady()) {
            error = component.isLoading() ? QStringLiteral("QML component load timed out")
                                          : component.errorString();
            QObject::disconnect(warningConnection);
            return nullptr;
        }
        auto object = std::unique_ptr<QObject>(component.create());
        if (!object) {
            for (const auto& componentError : component.errors()) {
                diagnostics.push_back(componentError.toString());
            }
            error = diagnostics.join(QLatin1Char('\n'));
        }
        QObject::disconnect(warningConnection);
        return object;
    }

    [[nodiscard]] QVariantList variantList(const QVariant& value) {
        if (value.metaType() == QMetaType::fromType<QJSValue>()) {
            return value.value<QJSValue>().toVariant().toList();
        }
        return value.toList();
    }

    [[nodiscard]] bool isNullValue(const QVariant& value) {
        if (!value.isValid() || value.isNull()) {
            return true;
        }
        if (value.metaType() == QMetaType::fromType<QJSValue>()) {
            const auto result = value.value<QJSValue>();
            return result.isNull() || result.isUndefined();
        }
        return false;
    }

    [[nodiscard]] QAccessibleInterface* accessibleByName(QQuickItem& root, const QString& name) {
        auto* interface = QAccessible::queryAccessibleInterface(&root);
        if (interface != nullptr && interface->text(QAccessible::Name) == name) {
            return interface;
        }
        for (auto* child : root.childItems()) {
            if (auto* match = accessibleByName(*child, name)) {
                return match;
            }
        }
        return nullptr;
    }

    class ActivityAnalyticsQmlTest final : public QObject {
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
            QAccessible::setActive(true);
        }

        void cleanupTestCase() {
            QAccessible::setActive(false);
        }

        void simple_bars_preserve_missing_values_and_expose_accessible_table() {
            QQuickWindow window;
            window.setGeometry(0, 0, 640, 420);
            QQmlEngine engine;
            QString error;
            auto chart = loadQml(engine, kSimpleBarHarness, error);
            QVERIFY2(chart != nullptr, qPrintable(error));
            auto* chartItem = qobject_cast<QQuickItem*>(chart.get());
            QVERIFY(chartItem != nullptr);
            chartItem->setParentItem(window.contentItem());

            QSignalSpy frameSpy(&window, &QQuickWindow::frameSwapped);
            window.show();
            window.requestUpdate();
            QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!frameSpy.isEmpty(), 1000);
            const QImage image = window.grabWindow();
            QVERIFY(!image.isNull());

            QCOMPARE(chart->property("chartType").toString(), QStringLiteral("bar"));
            QCOMPARE(chart->property("hasData").toBool(), true);
            QCOMPARE(chart->property("axisMinimum").toDouble(), 0.0);
            QCOMPARE(chart->property("axisMaximum").toDouble(), 5.0);

            const QVariantList rows = variantList(chart->property("tableRows"));
            QCOMPARE(rows.size(), 2);
            const QVariantMap firstRow = rows.at(0).toMap();
            const QVariantMap secondRow = rows.at(1).toMap();
            QCOMPARE(firstRow.value(QStringLiteral("category")).toString(), QStringLiteral("SEE"));
            QCOMPARE(variantList(firstRow.value(QStringLiteral("values"))).at(0).toString(),
                     QStringLiteral("5"));
            QCOMPARE(secondRow.value(QStringLiteral("category")).toString(),
                     QStringLiteral("Nao atribuido"));
            QCOMPARE(variantList(secondRow.value(QStringLiteral("values"))).at(0).toString(),
                     QStringLiteral("Sem dado"));

            auto* table =
                accessibleByName(*chartItem, QStringLiteral("Tabela textual dos dados do grafico"));
            QVERIFY(table != nullptr);
            QCOMPARE(table->role(), QAccessible::Pane);
            QVERIFY(accessibleByName(*chartItem, QStringLiteral("SEE: 5")) != nullptr);
            QVERIFY(accessibleByName(*chartItem, QStringLiteral("Nao atribuido: Sem dado")) !=
                    nullptr);
        }

        void stacked_and_percent_bars_reconcile_totals() {
            QQmlEngine engine;
            QString error;
            auto harness = loadQml(engine, kStackedBarHarness, error);
            QVERIFY2(harness != nullptr, qPrintable(error));
            auto* stacked = harness->findChild<QObject*>(QStringLiteral("stackedChart"));
            auto* percent = harness->findChild<QObject*>(QStringLiteral("percentChart"));
            QVERIFY(stacked != nullptr);
            QVERIFY(percent != nullptr);

            QCOMPARE(stacked->property("chartType").toString(), QStringLiteral("stackedBar"));
            QCOMPARE(stacked->property("axisMinimum").toDouble(), 0.0);
            QCOMPARE(stacked->property("axisMaximum").toDouble(), 7.0);
            QVariant firstTotal;
            QVERIFY(QMetaObject::invokeMethod(stacked, "categoryTotal",
                                              Q_RETURN_ARG(QVariant, firstTotal),
                                              Q_ARG(QVariant, QVariant{0})));
            QCOMPARE(firstTotal.toDouble(), 7.0);

            QCOMPARE(percent->property("chartType").toString(),
                     QStringLiteral("percentStackedBar"));
            QCOMPARE(percent->property("axisMinimum").toDouble(), 0.0);
            QCOMPARE(percent->property("axisMaximum").toDouble(), 100.0);
            QVariant firstPercentage;
            QVariant secondPercentage;
            QVERIFY(QMetaObject::invokeMethod(
                percent, "normalizedValue", Q_RETURN_ARG(QVariant, firstPercentage),
                Q_ARG(QVariant, QVariant{0}), Q_ARG(QVariant, QVariant{0})));
            QVERIFY(QMetaObject::invokeMethod(
                percent, "normalizedValue", Q_RETURN_ARG(QVariant, secondPercentage),
                Q_ARG(QVariant, QVariant{0}), Q_ARG(QVariant, QVariant{1})));
            QCOMPARE(firstPercentage.toDouble(), 25.0);
            QCOMPARE(secondPercentage.toDouble(), 75.0);
        }

        void trend_line_preserves_gap_and_uses_automatic_scale() {
            QQmlEngine engine;
            QString error;
            auto chart = loadQml(engine, kTrendLineHarness, error);
            QVERIFY2(chart != nullptr, qPrintable(error));
            QCOMPARE(chart->property("chartType").toString(), QStringLiteral("trendLine"));
            QCOMPARE(chart->property("axisMinimum").toDouble(), 10.0);
            QCOMPARE(chart->property("axisMaximum").toDouble(), 30.0);

            QVariant gap;
            QVariant lastValue;
            QVariant trend;
            QVERIFY(QMetaObject::invokeMethod(chart.get(), "valueAt", Q_RETURN_ARG(QVariant, gap),
                                              Q_ARG(QVariant, QVariant{0}),
                                              Q_ARG(QVariant, QVariant{1})));
            QVERIFY(QMetaObject::invokeMethod(
                chart.get(), "valueAt", Q_RETURN_ARG(QVariant, lastValue),
                Q_ARG(QVariant, QVariant{0}), Q_ARG(QVariant, QVariant{2})));
            QVERIFY(QMetaObject::invokeMethod(
                chart.get(), "trendValueAt", Q_RETURN_ARG(QVariant, trend),
                Q_ARG(QVariant, QVariant{0}), Q_ARG(QVariant, QVariant{1})));
            QVERIFY(isNullValue(gap));
            QCOMPARE(lastValue.toDouble(), 30.0);
            QCOMPARE(trend.toDouble(), 20.0);
        }

        void long_category_labels_use_measured_spacing() {
            QQmlEngine engine;
            QString error;
            auto chart = loadQml(engine, kLongCategoryHarness, error);
            QVERIFY2(chart != nullptr, qPrintable(error));

            QVariant stride;
            QVERIFY(QMetaObject::invokeMethod(chart.get(), "categoryLabelStride",
                                              Q_RETURN_ARG(QVariant, stride),
                                              Q_ARG(QVariant, QVariant{300.0})));
            QVERIFY(stride.toInt() > 1);

            QVariant indices;
            QVERIFY(QMetaObject::invokeMethod(chart.get(), "categoryLabelIndices",
                                              Q_RETURN_ARG(QVariant, indices),
                                              Q_ARG(QVariant, stride)));
            const auto labelIndices = variantList(indices);
            QVERIFY(labelIndices.size() < 6);
            QCOMPARE(labelIndices.front().toInt(), 1);
            QCOMPARE(labelIndices.back().toInt(), 4);

            QVariant lines;
            QVERIFY(QMetaObject::invokeMethod(chart.get(), "categoryLabelLines",
                                              Q_RETURN_ARG(QVariant, lines),
                                              Q_ARG(QVariant, QVariant{"IEE\nIEE2"})));
            const auto labelLines = variantList(lines);
            QCOMPARE(labelLines.size(), 2);
            QCOMPARE(labelLines[0].toString(), QStringLiteral("IEE"));
            QCOMPARE(labelLines[1].toString(), QStringLiteral("IEE2"));
        }

        void resize_and_refresh_update_derived_models() {
            QQmlEngine engine;
            QString error;
            auto chart = loadQml(engine, kSimpleBarHarness, error);
            QVERIFY2(chart != nullptr, qPrintable(error));
            auto* chartItem = qobject_cast<QQuickItem*>(chart.get());
            QVERIFY(chartItem != nullptr);
            const double originalPlotWidth = chart->property("plotWidth").toDouble();

            chartItem->setWidth(420);
            QCoreApplication::processEvents();
            QVERIFY(chart->property("plotWidth").toDouble() < originalPlotWidth);

            QVERIFY(QMetaObject::invokeMethod(chart.get(), "mutateFirstValue"));
            QTRY_COMPARE_WITH_TIMEOUT(chart->property("axisMaximum").toDouble(), 9.0, 1000);
            const QVariantList rows = variantList(chart->property("tableRows"));
            QCOMPARE(
                variantList(rows.at(0).toMap().value(QStringLiteral("values"))).at(0).toString(),
                QStringLiteral("9"));
        }
    };

} // namespace

QTEST_MAIN(ActivityAnalyticsQmlTest)

#include "ActivityAnalyticsQmlTest.moc"
