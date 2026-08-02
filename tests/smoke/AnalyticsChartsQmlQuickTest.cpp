#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QtQuickTest/quicktest.h>

#include <array>

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

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    class Setup final : public QObject {
        Q_OBJECT

      public slots:
        void qmlEngineAvailable(QQmlEngine* engine) {
            const QDir root = repositoryRoot();
            const QUrl themeUrl =
                QUrl::fromLocalFile(root.filePath(QStringLiteral("app/desktop/qml/Theme.qml")));
            qmlRegisterSingletonType(themeUrl, "SsaConsultaRapida", 1, 0, "Theme");

            const QDir analytics(root.filePath(QStringLiteral("app/desktop/qml/analytics")));
            for (const auto& registration : kAnalyticsTypes) {
                const QUrl url = QUrl::fromLocalFile(
                    analytics.filePath(QString::fromUtf8(registration.fileName)));
                qmlRegisterType(url, "SsaConsultaRapida", 1, 0, registration.typeName);
            }

            engine->addImportPath(root.filePath(QStringLiteral("app/desktop/qml")));
            engine->addImportPath(analytics.path());
        }
    };

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(AnalyticsCharts, Setup)

#include "AnalyticsChartsQmlQuickTest.moc"
