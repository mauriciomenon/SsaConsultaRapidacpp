#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqml.h>

#include <sys/resource.h>

#include <memory>

namespace {

    constexpr int valueCount = 5000;
    constexpr int readyTimeoutMs = 10000;

    bool registerQmlTypes() {
        const QDir qmlRoot(QStringLiteral(SSA_SOURCE_DIR "/app/desktop/qml"));
        const QDir components(qmlRoot.filePath(QStringLiteral("components")));
        return qmlRegisterSingletonType(QUrl::fromLocalFile(qmlRoot.filePath("Theme.qml")),
                                        "SsaConsultaRapida", 1, 0, "Theme") >= 0 &&
               qmlRegisterType(QUrl::fromLocalFile(components.filePath("ActionButton.qml")),
                               "SsaConsultaRapida", 1, 0, "ActionButton") >= 0 &&
               qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppTextField.qml")),
                               "SsaConsultaRapida", 1, 0, "AppTextField") >= 0 &&
               qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppCheckBox.qml")),
                               "SsaConsultaRapida", 1, 0, "AppCheckBox") >= 0 &&
               qmlRegisterType(
                   QUrl::fromLocalFile(components.filePath("AdvancedTextValuePopup.qml")),
                   "SsaConsultaRapida", 1, 0, "AdvancedTextValuePopup") >= 0;
    }

    int qmlMetric(QObject& object, const char* method) {
        QVariant result;
        if (!QMetaObject::invokeMethod(&object, method, Q_RETURN_ARG(QVariant, result))) {
            return -1;
        }
        return result.toInt();
    }

    bool waitUntilReady(QGuiApplication& application, QObject& harness, const bool eager) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < readyTimeoutMs) {
            application.processEvents(QEventLoop::AllEvents, 10);
            const int created = qmlMetric(harness, "createdCount");
            const int active = qmlMetric(harness, "activeCount");
            if (created == valueCount && ((eager && active == valueCount) ||
                                          (!eager && active > 0 && active < valueCount))) {
                return true;
            }
        }
        return false;
    }

    long peakRss() {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return -1;
        }
        return usage.ru_maxrss;
    }

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption modeOption(QStringLiteral("mode"), QStringLiteral("eager or virtual"),
                                        QStringLiteral("mode"));
    parser.addOption(modeOption);
    parser.process(application);
    const QString mode = parser.value(modeOption);
    if (mode != QStringLiteral("eager") && mode != QStringLiteral("virtual")) {
        qCritical("error: --mode must be eager or virtual");
        return 2;
    }
    if (!registerQmlTypes()) {
        qCritical("error: QML type registration failed");
        return 2;
    }

    QStringList values;
    values.reserve(valueCount);
    for (int index = 0; index < valueCount; ++index) {
        values.push_back(QStringLiteral("value-%1").arg(index));
    }

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("probeValues"), values);
    engine.rootContext()->setContextProperty(QStringLiteral("probeEager"),
                                             mode == QStringLiteral("eager"));
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QtQuick.Controls
        import SsaConsultaRapida

        Item {
            id: root
            width: 600
            height: 500
            readonly property bool eagerMode: probeEager

            Button {
                id: trigger
                x: 100
                y: 100
                width: 30
                height: 30
            }

            AdvancedTextValuePopup {
                id: popup
                trigger: trigger
                columnKey: "situacao"
                columnLabel: "Situacao"
                allValues: probeValues
                valuesLoading: false
                maxValueLength: 10
                textFilter: ""
            }

            Column {
                visible: root.eagerMode

                Repeater {
                    id: eagerRepeater
                    model: root.eagerMode ? probeValues : []

                    delegate: Row {
                        required property string modelData
                        width: 300
                        height: 22
                        spacing: 8

                        Text {
                            width: 200
                            text: modelData
                        }
                        AppCheckBox {
                            width: 32
                            height: 22
                        }
                        AppCheckBox {
                            width: 32
                            height: 22
                        }
                    }
                }
            }

            function start() {
                if (!root.eagerMode)
                    popup.openForCurrentFilter();
            }

            function createdCount() {
                return root.eagerMode ? eagerRepeater.count : popup.optionList.count;
            }

            function activeCount() {
                if (!root.eagerMode)
                    return popup.activeDelegateCount();
                var count = 0;
                for (var index = 0; index < eagerRepeater.count; ++index) {
                    if (eagerRepeater.itemAt(index) !== null)
                        ++count;
                }
                return count;
            }
        }
    )QML",
                      QUrl(QStringLiteral("inmemory:/AdvancedPopupMemoryProbe.qml")));
    QElapsedTimer componentTimer;
    componentTimer.start();
    while (component.status() == QQmlComponent::Loading &&
           componentTimer.elapsed() < readyTimeoutMs) {
        application.processEvents(QEventLoop::AllEvents, 10);
    }
    if (!component.isReady()) {
        qCritical().noquote() << component.errorString();
        return 2;
    }

    QQuickWindow window;
    window.setGeometry(0, 0, 600, 500);
    std::unique_ptr<QObject> harness(component.create());
    auto* harnessItem = qobject_cast<QQuickItem*>(harness.get());
    if (harnessItem == nullptr) {
        qCritical().noquote() << component.errorString();
        return 2;
    }
    harnessItem->setParentItem(window.contentItem());
    window.show();
    if (!QMetaObject::invokeMethod(harness.get(), "start") ||
        !waitUntilReady(application, *harness, mode == QStringLiteral("eager"))) {
        qCritical("error: QML probe did not become ready");
        return 2;
    }

    const long rss = peakRss();
    if (rss <= 0) {
        qCritical("error: peak RSS is unavailable");
        return 2;
    }
    qInfo().noquote() << QStringLiteral("QML_POPUP_RSS mode=%1 count=%2 active=%3 rss=%4")
                             .arg(mode)
                             .arg(qmlMetric(*harness, "createdCount"))
                             .arg(qmlMetric(*harness, "activeCount"))
                             .arg(rss);
    return 0;
}
