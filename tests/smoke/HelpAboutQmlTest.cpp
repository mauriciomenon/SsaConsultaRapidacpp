#include "PresentationSmokeFakes.h"

#include "presentation/MainViewModel.h"
#include "query/SsaQueryService.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>
#include <QtQml/qqml.h>

#include <memory>

namespace {

    QDir repositoryRoot() {
        return {QStringLiteral(SSA_SOURCE_DIR)};
    }

    class SourceQmlUrlInterceptor final : public QQmlAbstractUrlInterceptor {
      public:
        QUrl intercept(const QUrl& url, DataType) override {
            const QString prefix = QStringLiteral("qrc:/qt/qml/SsaConsultaRapida/app/desktop/qml/");
            const QString value = url.toString();
            if (!value.startsWith(prefix)) {
                return url;
            }
            return QUrl::fromLocalFile(repositoryRoot().filePath(
                QStringLiteral("app/desktop/qml/") + value.sliced(prefix.size())));
        }
    };

    std::unique_ptr<QObject> loadDialog(QQmlEngine& engine, const QString& fileName,
                                        QString& error) {
        const QDir components(
            repositoryRoot().filePath(QStringLiteral("app/desktop/qml/components")));
        QQmlComponent component(&engine, QUrl::fromLocalFile(components.filePath(fileName)));
        if (!component.isReady()) {
            error = component.errorString();
            return nullptr;
        }
        auto dialog = std::unique_ptr<QObject>(component.create());
        if (!dialog) {
            error = component.errorString();
        }
        return dialog;
    }

    QString readSource(const QString& relativePath) {
        QFile source(repositoryRoot().filePath(relativePath));
        if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(source.readAll());
    }

    QWindow* waitForVisibleWindow(const QString& title) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1000) {
            QCoreApplication::processEvents();
            for (auto* window : QGuiApplication::topLevelWindows()) {
                if (window->isVisible() && window->title() == title) {
                    return window;
                }
            }
            QTest::qWait(10);
        }
        return nullptr;
    }

    bool waitForDestruction(const QPointer<QWindow>& window) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1000 && window) {
            QCoreApplication::processEvents();
            QTest::qWait(10);
        }
        return window.isNull();
    }

    bool captureDialogScreenshot(QObject& dialog, const QString& fileName) {
        auto* window = qobject_cast<QQuickWindow*>(&dialog);
        if (window == nullptr) {
            return false;
        }
        QSignalSpy frameSpy(window, &QQuickWindow::frameSwapped);
        window->show();
        window->requestUpdate();
        if (frameSpy.isEmpty() && !frameSpy.wait(1000)) {
            return false;
        }
        const auto image = window->grabWindow();
        window->hide();
        return !image.isNull() &&
               image.save(QDir(QCoreApplication::applicationDirPath()).filePath(fileName));
    }

    bool dialogCanOpenCloseAndReopen(QObject& mainWindow, const char* openMethod,
                                     const QString& title) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (!QMetaObject::invokeMethod(&mainWindow, openMethod)) {
                return false;
            }
            auto* dialog = waitForVisibleWindow(title);
            if (!dialog) {
                return false;
            }
            QPointer<QWindow> guard(dialog);
            dialog->close();
            if (!waitForDestruction(guard)) {
                return false;
            }
        }
        return true;
    }

    class HelpAboutQmlTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            const QUrl themeUrl = QUrl::fromLocalFile(
                repositoryRoot().filePath(QStringLiteral("app/desktop/qml/Theme.qml")));
            QVERIFY(qmlRegisterSingletonType(themeUrl, "SsaConsultaRapida", 1, 0, "Theme") >= 0);
        }

        void help_dialog_exposes_cpp_query_contract() {
            QQmlEngine engine;
            QString error;
            const auto dialog = loadDialog(engine, QStringLiteral("HelpDialog.qml"), error);
            QVERIFY2(dialog != nullptr, qPrintable(error));

            const QString helpText = dialog->property("helpText").toString();
            QVERIFY(helpText.contains(QStringLiteral("Virgula")));
            QVERIFY(helpText.contains(QStringLiteral("!termo")));
            QVERIFY(helpText.contains(QStringLiteral("=termo")));
            QVERIFY(helpText.contains(QStringLiteral("Filtros por coluna")));
            QVERIFY(helpText.contains(QStringLiteral("Filtros avancados")));
            QVERIFY(captureDialogScreenshot(*dialog, QStringLiteral("help-dialog.png")));
        }

        void about_dialog_uses_runtime_application_version() {
            QGuiApplication::setApplicationVersion(QStringLiteral("9.8.7-test"));
            QQmlEngine engine;
            QString error;
            const auto dialog = loadDialog(engine, QStringLiteral("AboutDialog.qml"), error);
            QVERIFY2(dialog != nullptr, qPrintable(error));

            QCOMPARE(dialog->property("productName").toString(),
                     QStringLiteral("SSA Consulta Rapida"));
            QCOMPARE(dialog->property("authorName").toString(), QStringLiteral("Mauricio Menon"));
            QCOMPARE(dialog->property("productVersion").toString(), QStringLiteral("9.8.7-test"));
            QVERIFY(captureDialogScreenshot(*dialog, QStringLiteral("about-dialog.png")));
        }

        void help_menu_keeps_installation_guide() {
            const QString mainQml = readSource(QStringLiteral("app/desktop/qml/Main.qml"));
            QVERIFY(!mainQml.isEmpty());
            QVERIFY(mainQml.contains(QStringLiteral("title: \"Ajuda\"")));
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Guia de instalacao\"")));
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Ajuda\"")));
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Sobre\"")));
            QVERIFY(mainQml.contains(QStringLiteral("id: helpDialogLoader")));
            QVERIFY(mainQml.contains(QStringLiteral("id: aboutDialogLoader")));
        }

        void sam_menu_exposes_refresh_configuration() {
            const QString mainQml = readSource(QStringLiteral("app/desktop/qml/Main.qml"));
            QVERIFY(!mainQml.isEmpty());
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Atualizar agora\"")));
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Atualizacao automatica\"")));
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Configurar atualizacao\"")));
            QVERIFY(mainQml.contains(QStringLiteral("id: samRefreshDialogLoader")));
        }

        void query_cancel_menu_uses_the_contextual_cancel_barrier() {
            const QString mainQml = readSource(QStringLiteral("app/desktop/qml/Main.qml"));
            QVERIFY(!mainQml.isEmpty());
            QVERIFY(mainQml.contains(QStringLiteral("text: \"Cancelar consulta\"")));
            QVERIFY(mainQml.contains(QStringLiteral("onTriggered: root.vm.requestCancelAll()")));
        }

        void sam_refresh_dialog_renders_offscreen_screenshot() {
            SourceQmlUrlInterceptor sourceInterceptor;
            QQmlEngine engine;
            engine.addUrlInterceptor(&sourceInterceptor);
            engine.addImportPath(QStringLiteral(SSA_BUILD_DIR));
            const QDir components(
                repositoryRoot().filePath(QStringLiteral("app/desktop/qml/components")));
            QQmlComponent component(&engine, QUrl::fromLocalFile(components.filePath(
                                                 QStringLiteral("SamRefreshDialog.qml"))));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            ssa::presentation::WorkflowCommandViewModel workflows(nullptr);
            QVariantMap properties;
            properties.insert(QStringLiteral("workflowViewModel"),
                              QVariant::fromValue<QObject*>(&workflows));
            const auto object =
                std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
            QVERIFY2(object != nullptr, qPrintable(component.errorString()));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            QSignalSpy frameSpy(window, &QQuickWindow::frameSwapped);

            window->show();
            window->requestUpdate();
            QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 0, 1000);
            const auto image = window->grabWindow();

            auto* settingsGrid =
                window->findChild<QQuickItem*>(QStringLiteral("samRefreshSettingsGrid"));
            auto* baseUrlField = window->findChild<QQuickItem*>(QStringLiteral("samBaseUrlField"));
            auto* intervalSpinBox =
                window->findChild<QQuickItem*>(QStringLiteral("samIntervalSpinBox"));
            QVERIFY(settingsGrid != nullptr);
            QVERIFY(baseUrlField != nullptr);
            QVERIFY(intervalSpinBox != nullptr);
            auto* intervalContent =
                qvariant_cast<QQuickItem*>(intervalSpinBox->property("contentItem"));
            QVERIFY(intervalContent != nullptr);

            const auto intervalTopLeft =
                intervalSpinBox->mapToItem(window->contentItem(), QPointF{});
            const QRectF intervalBounds{intervalTopLeft, intervalSpinBox->size()};
            const QRectF windowBounds{QPointF{}, window->contentItem()->size()};
            qInfo().nospace() << "VERIFY_SAM_DIALOG interval=" << intervalBounds
                              << " content_width=" << intervalContent->width()
                              << " content_implicit_width=" << intervalContent->implicitWidth()
                              << " reference_width=" << baseUrlField->width()
                              << " grid_width=" << settingsGrid->width()
                              << " window=" << windowBounds;

            QVERIFY(windowBounds.contains(intervalBounds));
            QVERIFY(intervalSpinBox->width() >= 140.0);
            QVERIFY(intervalContent->width() >= intervalContent->implicitWidth());

            QVERIFY(!image.isNull());
            QCOMPARE(image.size(), window->size());
            const auto outputPath = QDir(QCoreApplication::applicationDirPath())
                                        .filePath(QStringLiteral("sam-refresh-dialog.png"));
            QVERIFY(image.save(outputPath));
            window->hide();
        }

        void column_selector_popup_tracks_trigger_and_clamps_to_overlay() {
            auto repository = std::make_shared<ssa::tests::presentation_smoke::FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<ssa::tests::presentation_smoke::FakeCommands>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<ssa::tests::presentation_smoke::CapturingImportPort>(), nullptr,
                nullptr,
                std::make_shared<ssa::tests::presentation_smoke::CapturingDerivadasPort>());
            ssa::presentation::MainViewModel viewModel(service, commands, nullptr, nullptr,
                                                       workflows);
            SourceQmlUrlInterceptor sourceInterceptor;
            QQmlEngine engine;
            engine.addUrlInterceptor(&sourceInterceptor);
            engine.addImportPath(QStringLiteral(SSA_BUILD_DIR));
            QQmlComponent component(&engine, QUrl::fromLocalFile(repositoryRoot().filePath(
                                                 QStringLiteral("app/desktop/qml/Main.qml"))));
            QVariantMap initialProperties;
            initialProperties.insert(QStringLiteral("mainViewModel"),
                                     QVariant::fromValue<QObject*>(&viewModel));
            const auto mainWindow =
                std::unique_ptr<QObject>(component.createWithInitialProperties(initialProperties));
            QVERIFY2(mainWindow != nullptr, qPrintable(component.errorString()));
            auto* window = qobject_cast<QQuickWindow*>(mainWindow.get());
            QVERIFY(window != nullptr);
            window->resize(1180, 760);
            window->show();
            QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 1000);

            auto* mainTable = window->findChild<QObject*>(QStringLiteral("mainSsaTable"));
            auto* popup = window->findChild<QObject*>(QStringLiteral("columnSelectorPopup"));
            QVERIFY(mainTable != nullptr);
            QVERIFY(popup != nullptr);

            auto* cellMenu = window->findChild<QObject*>(QStringLiteral("cellContextMenu"));
            auto* cellAction =
                window->findChild<QQuickItem*>(QStringLiteral("cellConfigureColumnsAction"));
            auto* copyCellAction = window->findChild<QQuickItem*>(QStringLiteral("copyCellAction"));
            auto* copyRowAction = window->findChild<QQuickItem*>(QStringLiteral("copyRowAction"));
            auto* copySsaAction = window->findChild<QQuickItem*>(QStringLiteral("copySsaAction"));
            auto* openDetailsAction =
                window->findChild<QQuickItem*>(QStringLiteral("openDetailsAction"));
            QVERIFY(cellMenu != nullptr);
            QVERIFY(cellAction != nullptr);
            QVERIFY(copyCellAction != nullptr);
            QVERIFY(copyRowAction != nullptr);
            QVERIFY(copySsaAction != nullptr);
            QVERIFY(openDetailsAction != nullptr);
            QCOMPARE(copyCellAction->property("actionId").toString(), QString("copy_cell"));
            QCOMPARE(copyRowAction->property("actionId").toString(), QString("copy_row"));
            QCOMPARE(copySsaAction->property("actionId").toString(), QString("copy_ssa"));
            QCOMPARE(openDetailsAction->property("actionId").toString(), QString("open_details"));

            QSignalSpy copySpy(mainTable, SIGNAL(copyTextRequested(QString)));
            const auto triggerCopyAction = [&](QQuickItem* action, const char* property,
                                               const QString& value) {
                mainTable->setProperty(property, value);
                cellMenu->setProperty("x", 900);
                cellMenu->setProperty("y", 100);
                QVERIFY(QMetaObject::invokeMethod(cellMenu, "popup"));
                QTRY_VERIFY_WITH_TIMEOUT(action->isVisible(), 1000);
                const int expectedCount = copySpy.count() + 1;
                const auto actionCenter = action->mapToItem(
                    window->contentItem(), QPointF{action->width() / 2, action->height() / 2});
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, actionCenter.toPoint());
                QTRY_COMPARE_WITH_TIMEOUT(copySpy.count(), expectedCount, 1000);
                QCOMPARE(copySpy.back().at(0).toString(), value);
            };
            triggerCopyAction(copyCellAction, "contextCellText", QStringLiteral("cell value"));
            triggerCopyAction(copyRowAction, "contextRowText", QStringLiteral("row value"));
            triggerCopyAction(copySsaAction, "contextSsaNumber", QStringLiteral("202600001"));

            cellMenu->setProperty("x", 900);
            cellMenu->setProperty("y", 100);
            QVERIFY(QMetaObject::invokeMethod(cellMenu, "popup"));
            QTRY_VERIFY_WITH_TIMEOUT(cellMenu->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(cellAction->isVisible(), 1000);
            const auto cellActionCenter = cellAction->mapToItem(
                window->contentItem(), QPointF{cellAction->width() / 2, cellAction->height() / 2});
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, cellActionCenter.toPoint());
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!cellMenu->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!popup->property("visible").toBool(), 1000);

            const auto tableSource =
                readSource(QStringLiteral("app/desktop/qml/components/SsaTable.qml"));
            QVERIFY(tableSource.contains(
                QStringLiteral("root.configureColumnsRequested(headerConfigureColumnsAction)")));

            QQuickItem trigger(window->contentItem());
            trigger.setX(1040);
            trigger.setY(120);
            trigger.setWidth(100);
            trigger.setHeight(30);
            const QVariant triggerArgument = QVariant::fromValue<QObject*>(&trigger);
            QVERIFY(QMetaObject::invokeMethod(mainTable, "configureColumnsRequested",
                                              Q_ARG(QVariant, triggerArgument)));
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);

            auto* overlay = qvariant_cast<QQuickItem*>(popup->property("parent"));
            QVERIFY(overlay != nullptr);
            auto popupBounds =
                QRectF{popup->property("x").toReal(), popup->property("y").toReal(),
                       popup->property("width").toReal(), popup->property("height").toReal()};
            const auto triggerOrigin = trigger.mapToItem(overlay, QPointF{});
            auto overlayBounds = QRectF{QPointF{}, overlay->size()};
            QVERIFY(overlayBounds.contains(popupBounds));
            QVERIFY(qAbs(popupBounds.right() - (triggerOrigin.x() + trigger.width())) <= 1.0);
            QVERIFY(popupBounds.top() > triggerOrigin.y());
            const auto initialPopupX = popupBounds.x();

            trigger.setX(1340);
            trigger.setY(780);
            trigger.setWidth(120);
            window->resize(1500, 900);
            QTRY_COMPARE_WITH_TIMEOUT(window->width(), 1500, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(window->height(), 900, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(overlay->width(), 1500.0, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(overlay->height(), 900.0, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(
                qAbs(popup->property("x").toReal() + popup->property("width").toReal() -
                     (trigger.mapToItem(overlay, QPointF{}).x() + trigger.width())) <= 1.0,
                1000);

            overlay = qvariant_cast<QQuickItem*>(popup->property("parent"));
            QVERIFY(overlay != nullptr);
            popupBounds =
                QRectF{popup->property("x").toReal(), popup->property("y").toReal(),
                       popup->property("width").toReal(), popup->property("height").toReal()};
            const auto resizedTriggerOrigin = trigger.mapToItem(overlay, QPointF{});
            overlayBounds = QRectF{QPointF{}, overlay->size()};
            QVERIFY(overlayBounds.contains(popupBounds));
            QVERIFY(qAbs(popupBounds.right() - (resizedTriggerOrigin.x() + trigger.width())) <=
                    1.0);
            QVERIFY(popupBounds.top() < resizedTriggerOrigin.y());
            QVERIFY(popupBounds.x() != initialPopupX);
            window->hide();
        }

        void main_window_opens_closes_and_reopens_help_and_about() {
            auto repository = std::make_shared<ssa::tests::presentation_smoke::FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<ssa::tests::presentation_smoke::FakeCommands>();
            auto derivadasPort =
                std::make_shared<ssa::tests::presentation_smoke::CapturingDerivadasPort>();
            derivadasPort->setLegacySpreadsheetConverterAvailable(false);
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<ssa::tests::presentation_smoke::CapturingImportPort>(), nullptr,
                nullptr, derivadasPort);
            ssa::presentation::MainViewModel viewModel(service, commands, nullptr, nullptr,
                                                       workflows);
            SourceQmlUrlInterceptor sourceInterceptor;
            QQmlEngine engine;
            engine.addUrlInterceptor(&sourceInterceptor);
            engine.addImportPath(QStringLiteral(SSA_BUILD_DIR));
            QQmlComponent component(&engine, QUrl::fromLocalFile(repositoryRoot().filePath(
                                                 QStringLiteral("app/desktop/qml/Main.qml"))));
            QVariantMap initialProperties;
            initialProperties.insert(QStringLiteral("mainViewModel"),
                                     QVariant::fromValue<QObject*>(&viewModel));
            const auto mainWindow =
                std::unique_ptr<QObject>(component.createWithInitialProperties(initialProperties));
            QVERIFY2(mainWindow != nullptr, qPrintable(component.errorString()));

            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openHelpDialog",
                                                QStringLiteral("Ajuda")));
            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openAboutDialog",
                                                QStringLiteral("Sobre")));
            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openSamRefreshDialog",
                                                QStringLiteral("Atualizacao SAM")));

            auto* openDatabase =
                mainWindow->findChild<QObject*>(QStringLiteral("openDatabaseMenuItem"));
            QVERIFY(openDatabase != nullptr);
            QVERIFY(QMetaObject::invokeMethod(openDatabase, "triggered"));
            auto* databaseDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("databaseFileDialog"));
            QVERIFY(databaseDialog != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(databaseDialog->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(databaseDialog, "close"));

            auto* openImportData =
                mainWindow->findChild<QObject*>(QStringLiteral("openImportDataMenuItem"));
            QVERIFY(openImportData != nullptr);
            QVERIFY(QMetaObject::invokeMethod(openImportData, "triggered"));
            auto* importDataDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("importDataFileDialog"));
            QVERIFY(importDataDialog != nullptr);
            QCOMPARE(importDataDialog->property("title").toString(),
                     QStringLiteral("Importar XLSX externo"));
            QCOMPARE(importDataDialog->property("nameFilters").toStringList(),
                     QStringList{QStringLiteral("Planilhas XLSX (*.xlsx)")});
            QVERIFY(QMetaObject::invokeMethod(importDataDialog, "close"));

            auto* openImportDerivations =
                mainWindow->findChild<QObject*>(QStringLiteral("openImportDerivationsMenuItem"));
            QVERIFY(openImportDerivations != nullptr);
            QVERIFY(QMetaObject::invokeMethod(openImportDerivations, "triggered"));
            auto* derivadasDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("derivadasFileDialog"));
            QVERIFY(derivadasDialog != nullptr);
            QCOMPARE(derivadasDialog->property("title").toString(),
                     QStringLiteral("Importar derivadas"));
            QCOMPARE(derivadasDialog->property("nameFilters").toStringList(),
                     QStringList({QStringLiteral("Derivadas (*.csv *.txt *.tsv *.xlsx *.xlsm)"),
                                  QStringLiteral("XLS legado - requer LibreOffice (*.xls)")}));
            auto* fileWorkflowDialogs =
                mainWindow->findChild<QObject*>(QStringLiteral("fileWorkflowDialogs"));
            QVERIFY(fileWorkflowDialogs != nullptr);
            const QVariant files = QVariant::fromValue(QVariantList{QUrl::fromLocalFile(
                QDir::temp().filePath(QStringLiteral("derivadas-legado.xls")))});
            QVERIFY(QMetaObject::invokeMethod(fileWorkflowDialogs, "requestDerivadasImport",
                                              Q_ARG(QVariant, files)));
            auto* legacyPreflight =
                mainWindow->findChild<QObject*>(QStringLiteral("legacyDerivadasPreflightDialog"));
            QVERIFY(legacyPreflight != nullptr);
            QCOMPARE(legacyPreflight->property("title").toString(),
                     QStringLiteral("Importar XLS legado"));
            QVERIFY(legacyPreflight->property("text").toString().contains(
                QStringLiteral("LibreOffice")));
            auto* legacyUnavailable =
                mainWindow->findChild<QObject*>(QStringLiteral("legacyDerivadasUnavailableDialog"));
            QVERIFY(legacyUnavailable != nullptr);
            QCOMPARE(legacyUnavailable->property("title").toString(),
                     QStringLiteral("LibreOffice nao encontrado"));
            QVERIFY(legacyUnavailable->property("text").toString().contains(
                QStringLiteral("LibreOffice")));
            QTRY_VERIFY_WITH_TIMEOUT(legacyUnavailable->property("visible").toBool(), 1000);
            QCOMPARE(derivadasPort->importRequests().size(), std::size_t{0});
            QVERIFY(QMetaObject::invokeMethod(legacyUnavailable, "close"));

            viewModel.databaseSwitch()->openDatabase(
                QUrl{QStringLiteral("https://example.invalid/not-local.db")});
            auto* errorDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("databaseErrorDialog"));
            QVERIFY(errorDialog != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(errorDialog->property("visible").toBool(), 1000);
            QVERIFY(errorDialog->property("text").toString().contains(
                QStringLiteral("arquivo de banco local")));
            QVERIFY(QMetaObject::invokeMethod(errorDialog, "close"));

            auto* cancelButton =
                mainWindow->findChild<QObject*>(QStringLiteral("statusCancelButton"));
            auto* forceShutdownDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("forceShutdownDialog"));
            QVERIFY(cancelButton != nullptr);
            QVERIFY(forceShutdownDialog != nullptr);

            viewModel.browse()->status()->setLoading(true);
            QTRY_VERIFY_WITH_TIMEOUT(cancelButton->property("visible").toBool(), 1000);
            QCOMPARE(cancelButton->property("text").toString(), QStringLiteral("Cancelar"));
            QVERIFY(captureDialogScreenshot(*mainWindow, QStringLiteral("status-cancel.png")));

            viewModel.requestCancelAll();
            QTRY_COMPARE_WITH_TIMEOUT(cancelButton->property("text").toString(),
                                      QStringLiteral("Cancelando..."), 1000);
            QVERIFY(captureDialogScreenshot(*mainWindow, QStringLiteral("status-canceling.png")));

            QVERIFY(QMetaObject::invokeMethod(forceShutdownDialog, "open"));
            QTRY_VERIFY_WITH_TIMEOUT(forceShutdownDialog->property("visible").toBool(), 1000);
            QVERIFY(captureDialogScreenshot(*mainWindow, QStringLiteral("force-shutdown.png")));
            QVERIFY(QMetaObject::invokeMethod(forceShutdownDialog, "close"));
            viewModel.browse()->status()->setLoading(false);
        }
    };

} // namespace

QTEST_MAIN(HelpAboutQmlTest)

#include "HelpAboutQmlTest.moc"
