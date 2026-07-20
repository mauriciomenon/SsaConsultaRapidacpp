#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QTest>
#include <QtQml/qqml.h>

#include <algorithm>
#include <memory>

namespace {

    [[nodiscard]] QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    class FakeWorkflowViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)

      public:
        [[nodiscard]] bool canCancel() const noexcept {
            return canCancel_;
        }

        [[nodiscard]] int cancelCount() const noexcept {
            return cancelCount_;
        }

        Q_INVOKABLE void cancel() {
            ++cancelCount_;
            canCancel_ = false;
            emit stateChanged();
        }

        void start(const QString& operationLabel) {
            canCancel_ = true;
            emit stateChanged();
            emit progressSessionStarted(operationLabel);
        }

        void progress(const int percentage, const QString& status, const int currentFile,
                      const int totalFiles, const QString& fileName) {
            emit progressChanged(percentage, status, currentFile, totalFiles, fileName);
        }

        void output(const QString& line) {
            emit progressOutputLine(line);
        }

        void error(const QString& line) {
            emit progressErrorLine(line);
        }

        void finish(const bool succeeded, const bool canceled, const QString& message) {
            canCancel_ = false;
            emit stateChanged();
            emit progressSessionFinished(succeeded, canceled, message);
        }

      signals:
        void stateChanged();
        void progressSessionStarted(const QString& operationLabel);
        void progressChanged(int percentage, const QString& status, int currentFile, int totalFiles,
                             const QString& fileName);
        void progressOutputLine(const QString& line);
        void progressErrorLine(const QString& line);
        void progressSessionFinished(bool succeeded, bool canceled, const QString& message);

      private:
        bool canCancel_ = false;
        int cancelCount_ = 0;
    };

    class WorkflowProgressDialogQmlTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            const auto root = repositoryRoot();
            const auto qmlRoot = QDir(root.filePath(QStringLiteral("app/desktop/qml")));
            const auto components = QDir(qmlRoot.filePath(QStringLiteral("components")));
            QVERIFY(qmlRegisterSingletonType(
                        QUrl::fromLocalFile(qmlRoot.filePath(QStringLiteral("Theme.qml"))),
                        "SsaConsultaRapida", 1, 0, "Theme") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(
                                        components.filePath(QStringLiteral("ActionButton.qml"))),
                                    "SsaConsultaRapida", 1, 0, "ActionButton") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath(
                                        QStringLiteral("WorkflowProgressDialog.qml"))),
                                    "SsaConsultaRapida", 1, 0, "WorkflowProgressDialog") >= 0);
        }

        void dialog_tracks_progress_cancel_and_terminal_close_data() {
            QTest::addColumn<int>("windowWidth");
            QTest::addColumn<int>("windowHeight");
            QTest::newRow("1580x940") << 1580 << 940;
            QTest::newRow("1180x760") << 1180 << 760;
        }

        void dialog_tracks_progress_cancel_and_terminal_close() {
            QFETCH(int, windowWidth);
            QFETCH(int, windowHeight);
            static constexpr auto kHarness = R"QML(
import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

ApplicationWindow {
    width: testWindowWidth
    height: testWindowHeight
    visible: true

    WorkflowProgressDialog {
        objectName: "workflowProgressDialog"
        workflowViewModel: fakeWorkflow
    }
}
)QML";

            FakeWorkflowViewModel workflow;
            QQmlEngine engine;
            engine.rootContext()->setContextProperty(QStringLiteral("fakeWorkflow"), &workflow);
            engine.rootContext()->setContextProperty(QStringLiteral("testWindowWidth"),
                                                     windowWidth);
            engine.rootContext()->setContextProperty(QStringLiteral("testWindowHeight"),
                                                     windowHeight);
            QQmlComponent component(&engine);
            component.setData(kHarness,
                              QUrl(QStringLiteral("inmemory:/WorkflowProgressHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> object(component.create());
            QVERIFY2(object != nullptr, qPrintable(component.errorString()));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->show();
            QTRY_VERIFY_WITH_TIMEOUT(window->width() > 0 && window->height() > 0, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 1000);
            const auto platformName = QGuiApplication::platformName();
            if (platformName != QStringLiteral("offscreen") &&
                platformName != QStringLiteral("minimal")) {
                window->requestActivate();
                QTRY_VERIFY_WITH_TIMEOUT(window->isActive(), 1000);
            }
            window->contentItem()->polish();
            window->requestUpdate();
            QCoreApplication::processEvents();

            auto* dialog = window->findChild<QObject*>(QStringLiteral("workflowProgressDialog"));
            auto* title = window->findChild<QObject*>(QStringLiteral("workflowProgressTitle"));
            auto* status = window->findChild<QQuickItem*>(QStringLiteral("workflowProgressStatus"));
            auto* file = window->findChild<QQuickItem*>(QStringLiteral("workflowProgressFile"));
            auto* progressBar =
                window->findChild<QQuickItem*>(QStringLiteral("workflowProgressBar"));
            auto* output = window->findChild<QQuickItem*>(QStringLiteral("workflowProgressOutput"));
            auto* errors = window->findChild<QObject*>(QStringLiteral("workflowProgressErrors"));
            auto* outputLabel =
                window->findChild<QQuickItem*>(QStringLiteral("workflowProgressOutputLabel"));
            auto* outputScroll =
                window->findChild<QQuickItem*>(QStringLiteral("workflowProgressOutputScroll"));
            auto* errorLabel =
                window->findChild<QQuickItem*>(QStringLiteral("workflowProgressErrorLabel"));
            auto* errorScroll =
                window->findChild<QQuickItem*>(QStringLiteral("workflowProgressErrorScroll"));
            auto* cancelButton =
                window->findChild<QObject*>(QStringLiteral("workflowProgressCancelButton"));
            auto* closeButton =
                window->findChild<QObject*>(QStringLiteral("workflowProgressCloseButton"));
            QVERIFY(dialog != nullptr);
            QVERIFY(title != nullptr);
            QVERIFY(status != nullptr);
            QVERIFY(file != nullptr);
            QVERIFY(progressBar != nullptr);
            QVERIFY(output != nullptr);
            QVERIFY(errors != nullptr);
            QVERIFY(outputLabel != nullptr);
            QVERIFY(outputScroll != nullptr);
            QVERIFY(errorLabel != nullptr);
            QVERIFY(errorScroll != nullptr);
            QVERIFY(cancelButton != nullptr);
            QVERIFY(closeButton != nullptr);
            QCOMPARE(dialog->property("modal").toBool(), false);
            auto* dialogParent = qvariant_cast<QQuickItem*>(dialog->property("parent"));
            QVERIFY(dialogParent != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(dialogParent->width() > 0.0, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(dialogParent->height() > 0.0, 1000);

            workflow.start(QStringLiteral("Importacao em andamento"));
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 1000);
            QCOMPARE(dialog->property("title").toString(),
                     QStringLiteral("Importacao em andamento"));
            QVERIFY(!closeButton->property("enabled").toBool());
            QVERIFY(cancelButton->property("enabled").toBool());

            workflow.progress(40, QStringLiteral("Arquivo 3/10"), 3, 10,
                              QStringLiteral("lote_03.xlsx"));
            QCOMPARE(dialog->property("title").toString(),
                     QStringLiteral("Importacao em andamento - 3/10"));
            QCOMPARE(status->property("text").toString(), QStringLiteral("Arquivo 3/10"));
            QCOMPARE(progressBar->property("value").toInt(), 40);
            QCOMPARE(output->property("text").toString(), QString{});
            QCOMPARE(errors->property("text").toString(), QString{});

            const auto sceneRect = [](QQuickItem* item) {
                const auto topLeft = item->mapToScene(QPointF{0.0, 0.0});
                const auto bottomRight = item->mapToScene(QPointF{item->width(), item->height()});
                return QRectF{topLeft, bottomRight}.normalized();
            };
            const auto nearlyEqual = [](const qreal actual, const qreal expected) {
                return qAbs(actual - expected) <= 0.5;
            };
            const auto rectNearlyEqual = [&](const QRectF& actual, const QRectF& expected) {
                return nearlyEqual(actual.x(), expected.x()) &&
                       nearlyEqual(actual.y(), expected.y()) &&
                       nearlyEqual(actual.width(), expected.width()) &&
                       nearlyEqual(actual.height(), expected.height());
            };
            const auto layoutIsSettled = [&] {
                window->contentItem()->polish();
                window->requestUpdate();
                QCoreApplication::processEvents();
                const auto statusRect = sceneRect(status);
                const auto fileRect = sceneRect(file);
                const auto progressRect = sceneRect(progressBar);
                const auto outputLabelRect = sceneRect(outputLabel);
                const auto outputScrollRect = sceneRect(outputScroll);
                return file->isVisible() && statusRect.bottom() <= fileRect.top() + 0.5 &&
                       fileRect.bottom() <= progressRect.top() + 0.5 &&
                       progressRect.bottom() <= outputLabelRect.top() + 0.5 &&
                       outputLabelRect.bottom() <= outputScrollRect.top() + 0.5;
            };

            QTRY_VERIFY_WITH_TIMEOUT(layoutIsSettled(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(outputScroll->height() > 0.0, 1000);
            const auto baselineDialogWidth = dialog->property("width").toReal();
            const auto baselineDialogHeight = dialog->property("height").toReal();
            const auto baselineOutputLabel = sceneRect(outputLabel);
            const auto baselineOutputScroll = sceneRect(outputScroll);
            const auto baselineErrorLabel = sceneRect(errorLabel);
            const auto baselineErrorScroll = sceneRect(errorScroll);
            qInfo() << "workflow progress geometry baseline dialog" << baselineDialogWidth
                    << baselineDialogHeight << baselineOutputLabel << baselineOutputScroll
                    << baselineErrorLabel << baselineErrorScroll;
            QVERIFY(baselineDialogWidth > 0.0);
            QVERIFY(baselineDialogWidth <= 800.5);
            QVERIFY(baselineDialogHeight > 0.0);
            QVERIFY(baselineDialogHeight <= 620.5);
            QVERIFY(baselineOutputLabel.bottom() <= baselineOutputScroll.top() + 0.5);
            QVERIFY(baselineOutputScroll.bottom() <= baselineErrorLabel.top() + 0.5);
            QVERIFY(baselineErrorLabel.bottom() <= baselineErrorScroll.top() + 0.5);

            qreal previousContentHeight = output->property("contentHeight").toReal();
            qreal maxErrorGeometryDelta = 0.0;
            QString expectedOutput;
            for (int line = 1; line <= 100; ++line) {
                const auto outputLine = line == 1 ? QStringLiteral("arquivo validado")
                                                  : QStringLiteral("linha de saida %1").arg(line);
                expectedOutput +=
                    expectedOutput.isEmpty() ? outputLine : QStringLiteral("\n") + outputLine;
                workflow.output(outputLine);
                QTRY_COMPARE_WITH_TIMEOUT(output->property("text").toString(), expectedOutput,
                                          1000);
                window->contentItem()->polish();
                window->requestUpdate();
                QCoreApplication::processEvents();
                const auto contentHeight = output->property("contentHeight").toReal();
                QVERIFY(contentHeight + 0.5 >= previousContentHeight);
                previousContentHeight = contentHeight;
                const auto currentErrorLabel = sceneRect(errorLabel);
                const auto currentErrorScroll = sceneRect(errorScroll);
                const auto delta =
                    std::max({qAbs(currentErrorLabel.y() - baselineErrorLabel.y()),
                              qAbs(currentErrorLabel.height() - baselineErrorLabel.height()),
                              qAbs(currentErrorScroll.y() - baselineErrorScroll.y()),
                              qAbs(currentErrorScroll.height() - baselineErrorScroll.height())});
                maxErrorGeometryDelta = (std::max)(maxErrorGeometryDelta, delta);
                QVERIFY2(delta <= 0.5,
                         qPrintable(QStringLiteral("error geometry moved at output line %1 by %2")
                                        .arg(line)
                                        .arg(delta)));
            }
            QTRY_VERIFY_WITH_TIMEOUT(
                output->property("contentHeight").toReal() > outputScroll->height() + 0.5, 1000);
            const auto hundredLineOutputLabel = sceneRect(outputLabel);
            const auto hundredLineOutputScroll = sceneRect(outputScroll);
            const auto hundredLineErrorLabel = sceneRect(errorLabel);
            const auto hundredLineErrorScroll = sceneRect(errorScroll);
            const auto hundredLineDialogWidth = dialog->property("width").toReal();
            const auto hundredLineDialogHeight = dialog->property("height").toReal();
            qInfo() << "workflow progress geometry one hundred lines dialog"
                    << hundredLineDialogWidth << hundredLineDialogHeight << hundredLineOutputLabel
                    << hundredLineOutputScroll << hundredLineErrorLabel << hundredLineErrorScroll;
            QVERIFY(nearlyEqual(hundredLineDialogWidth, baselineDialogWidth));
            QVERIFY(nearlyEqual(hundredLineDialogHeight, baselineDialogHeight));
            QVERIFY(rectNearlyEqual(hundredLineOutputLabel, baselineOutputLabel));
            QVERIFY(rectNearlyEqual(hundredLineOutputScroll, baselineOutputScroll));
            QVERIFY(rectNearlyEqual(hundredLineErrorLabel, baselineErrorLabel));
            QVERIFY(rectNearlyEqual(hundredLineErrorScroll, baselineErrorScroll));

            qreal previousErrorContentHeight = errors->property("contentHeight").toReal();
            QString expectedErrors;
            for (int warning = 1; warning <= 100; ++warning) {
                const auto warningLine =
                    warning == 1 ? QStringLiteral("aviso terminal")
                                 : QStringLiteral("aviso de importacao %1").arg(warning);
                expectedErrors +=
                    expectedErrors.isEmpty() ? warningLine : QStringLiteral("\n") + warningLine;
                workflow.error(warningLine);
                QTRY_COMPARE_WITH_TIMEOUT(errors->property("text").toString(), expectedErrors,
                                          1000);
                window->contentItem()->polish();
                window->requestUpdate();
                QCoreApplication::processEvents();
                const auto errorContentHeight = errors->property("contentHeight").toReal();
                QVERIFY(errorContentHeight + 0.5 >= previousErrorContentHeight);
                previousErrorContentHeight = errorContentHeight;
                const auto currentOutputLabel = sceneRect(outputLabel);
                const auto currentOutputScroll = sceneRect(outputScroll);
                const auto currentErrorLabel = sceneRect(errorLabel);
                const auto currentErrorScroll = sceneRect(errorScroll);
                QVERIFY2(
                    rectNearlyEqual(currentOutputLabel, baselineOutputLabel),
                    qPrintable(QStringLiteral("output label moved at warning %1").arg(warning)));
                QVERIFY2(
                    rectNearlyEqual(currentOutputScroll, baselineOutputScroll),
                    qPrintable(QStringLiteral("output scroll moved at warning %1").arg(warning)));
                QVERIFY2(
                    rectNearlyEqual(currentErrorLabel, baselineErrorLabel),
                    qPrintable(QStringLiteral("error label moved at warning %1").arg(warning)));
                QVERIFY2(
                    rectNearlyEqual(currentErrorScroll, baselineErrorScroll),
                    qPrintable(QStringLiteral("error scroll moved at warning %1").arg(warning)));
            }
            QTRY_VERIFY_WITH_TIMEOUT(
                errors->property("contentHeight").toReal() > errorScroll->height() + 0.5, 1000);
            qInfo() << "workflow progress maximum error geometry delta" << maxErrorGeometryDelta;

            QTest::keyClick(window, Qt::Key_Escape);
            QVERIFY(dialog->property("visible").toBool());
            QVERIFY(QMetaObject::invokeMethod(cancelButton, "clicked"));
            QCOMPARE(workflow.cancelCount(), 1);
            QCOMPARE(status->property("text").toString(),
                     QStringLiteral("Cancelamento solicitado"));
            QVERIFY(!cancelButton->property("enabled").toBool());
            QVERIFY(dialog->property("visible").toBool());
            QVERIFY(QMetaObject::invokeMethod(cancelButton, "clicked"));
            QCOMPARE(workflow.cancelCount(), 1);

            workflow.progress(50, QStringLiteral("Nao substituir cancelamento"), 4, 10,
                              QStringLiteral("lote_04.xlsx"));
            QCOMPARE(status->property("text").toString(),
                     QStringLiteral("Cancelamento solicitado"));
            workflow.finish(false, true, QStringLiteral("Importacao cancelada"));
            QCOMPARE(status->property("text").toString(), QStringLiteral("Importacao cancelada"));
            QVERIFY(dialog->property("visible").toBool());
            QVERIFY(closeButton->property("enabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(closeButton, "clicked"));
            QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 1000);

            workflow.start(QStringLiteral("Reescaneamento em andamento"));
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 1000);
            QCOMPARE(output->property("text").toString(), QString{});
            QCOMPARE(errors->property("text").toString(), QString{});
            workflow.progress(90, QStringLiteral("Publicando banco validado"), 65, 65,
                              QStringLiteral("corpus.xlsx"));
            QCOMPARE(dialog->property("title").toString(),
                     QStringLiteral("Reescaneamento em andamento - 65/65"));
            workflow.finish(true, false, QStringLiteral("Reescaneamento concluido"));
            QCOMPARE(progressBar->property("value").toInt(), 100);
            QVERIFY(dialog->property("visible").toBool());
            QVERIFY(closeButton->property("enabled").toBool());
        }
    };

} // namespace

QTEST_MAIN(WorkflowProgressDialogQmlTest)

#include "WorkflowProgressDialogQmlTest.moc"
