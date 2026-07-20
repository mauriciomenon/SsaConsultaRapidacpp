#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QtQml/qqml.h>

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

        void dialog_tracks_progress_cancel_and_terminal_close() {
            static constexpr auto kHarness = R"QML(
import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

ApplicationWindow {
    width: 900
    height: 700
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

            auto* dialog = window->findChild<QObject*>(QStringLiteral("workflowProgressDialog"));
            auto* title = window->findChild<QObject*>(QStringLiteral("workflowProgressTitle"));
            auto* status = window->findChild<QObject*>(QStringLiteral("workflowProgressStatus"));
            auto* progressBar = window->findChild<QObject*>(QStringLiteral("workflowProgressBar"));
            auto* output = window->findChild<QObject*>(QStringLiteral("workflowProgressOutput"));
            auto* errors = window->findChild<QObject*>(QStringLiteral("workflowProgressErrors"));
            auto* cancelButton =
                window->findChild<QObject*>(QStringLiteral("workflowProgressCancelButton"));
            auto* closeButton =
                window->findChild<QObject*>(QStringLiteral("workflowProgressCloseButton"));
            QVERIFY(dialog != nullptr);
            QVERIFY(title != nullptr);
            QVERIFY(status != nullptr);
            QVERIFY(progressBar != nullptr);
            QVERIFY(output != nullptr);
            QVERIFY(errors != nullptr);
            QVERIFY(cancelButton != nullptr);
            QVERIFY(closeButton != nullptr);
            QCOMPARE(dialog->property("modal").toBool(), false);

            workflow.start(QStringLiteral("Importacao em andamento"));
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("visible").toBool(), 1000);
            QCOMPARE(dialog->property("title").toString(),
                     QStringLiteral("Importacao em andamento"));
            QVERIFY(!closeButton->property("enabled").toBool());
            QVERIFY(cancelButton->property("enabled").toBool());

            workflow.progress(40, QStringLiteral("Arquivo 3/10"), 3, 10,
                              QStringLiteral("lote_03.xlsx"));
            workflow.output(QStringLiteral("arquivo validado"));
            workflow.error(QStringLiteral("aviso de origem"));
            QCOMPARE(dialog->property("title").toString(),
                     QStringLiteral("Importacao em andamento - 3/10"));
            QCOMPARE(status->property("text").toString(), QStringLiteral("Arquivo 3/10"));
            QCOMPARE(progressBar->property("value").toInt(), 40);
            QCOMPARE(output->property("text").toString(), QStringLiteral("arquivo validado"));
            QCOMPARE(errors->property("text").toString(), QStringLiteral("aviso de origem"));

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
