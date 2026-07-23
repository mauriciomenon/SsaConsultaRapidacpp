#include "PresentationSmokeFakes.h"

#include "DesktopSmokeObjectNames.h"

#include "application/ActivityAnalyticsService.h"
#include "ports/IActivityAnalyticsPort.h"
#include "presentation/MainViewModel.h"
#include "query/SsaQueryService.h"

#include <QClipboard>
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
#include <stop_token>
#include <vector>

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

    QString captureDialogScreenshot(QObject& dialog, const QString& fileName) {
        auto* window = qobject_cast<QQuickWindow*>(&dialog);
        if (window == nullptr) {
            return QStringLiteral(
                       "screenshot target is not a QQuickWindow: objectName='%1' title='%2'")
                .arg(dialog.objectName(), dialog.property("title").toString());
        }
        const auto outputPath = QDir(QCoreApplication::applicationDirPath()).filePath(fileName);
        QSignalSpy frameSpy(window, &QQuickWindow::frameSwapped);
        window->show();
        window->requestUpdate();
        if (frameSpy.isEmpty() && !frameSpy.wait(1000)) {
            window->hide();
            return QStringLiteral("frameSwapped timed out after 1000ms: outputPath='%1'")
                .arg(outputPath);
        }
        const auto image = window->grabWindow();
        window->hide();
        if (image.isNull()) {
            return QStringLiteral("grabWindow returned a null image: outputPath='%1'")
                .arg(outputPath);
        }
        if (!image.save(outputPath)) {
            return QStringLiteral("failed to save screenshot: outputPath='%1'").arg(outputPath);
        }
        return {};
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

    class PassiveAnalyticsPort final : public ssa::ports::IActivityAnalyticsPort {
      public:
        ssa::domain::AnalyticsSeriesResult series(const ssa::domain::AnalyticsRequest&,
                                                  std::stop_token) const override {
            return {};
        }

        ssa::domain::AnalyticsDimensionValues dimensionValues(const ssa::domain::AnalyticsRequest&,
                                                              std::stop_token) const override {
            return {};
        }

        std::vector<ssa::domain::AnalyticsMetricAvailability>
        availability(std::stop_token) const override {
            return {};
        }
    };

    class HelpAboutQmlTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            QVERIFY(qmlRegisterSingletonType<ssa::app::desktop::DesktopSmokeObjectNames>(
                        "SsaConsultaRapida", 1, 0, "DesktopSmokeObjectNames",
                        [](QQmlEngine*, QJSEngine*) -> QObject* {
                            return new ssa::app::desktop::DesktopSmokeObjectNames;
                        }) >= 0);
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
            const auto screenshotError =
                captureDialogScreenshot(*dialog, QStringLiteral("help-dialog.png"));
            QVERIFY2(screenshotError.isEmpty(), qPrintable(screenshotError));
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
            const QString toolchainSupport = dialog->property("toolchainSupportText").toString();
            QCOMPARE(toolchainSupport,
                     QStringLiteral("Validado: Windows 11 amd64 - MSVC 19.51 | Debian/WSL "
                                    "amd64 - GCC 14.2\nHistorico: macOS arm64 - Apple Clang 21\n"
                                    "Reconhecidos sem gate: LLVM-MinGW 17.0.6 | MinGW GCC "
                                    "13.1.0/11.2.0\nNao suportado nesta versao: clang-cl 22.1.3 "
                                    "com Qt MSVC"));
            const auto screenshotError =
                captureDialogScreenshot(*dialog, QStringLiteral("about-dialog.png"));
            QVERIFY2(screenshotError.isEmpty(), qPrintable(screenshotError));
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

        void desktop_factory_composes_activity_analytics() {
            const QString factory =
                readSource(QStringLiteral("app/desktop/DesktopMainViewModelFactory.cpp"));
            QVERIFY(!factory.isEmpty());
            QVERIFY(
                factory.contains(QStringLiteral("SqliteActivityAnalyticsInitializer::initialize")));
            QVERIFY(factory.contains(QStringLiteral("SqliteSsaAnalyticsPort")));
            QVERIFY(factory.contains(QStringLiteral("ActivityAnalyticsService")));
            QVERIFY(factory.simplified().contains(QStringLiteral("analyticsPort, analyticsPort")));
            QVERIFY(factory.contains(QStringLiteral("analyticsService")));
        }

        void analytics_menu_is_disabled_without_service() {
            auto repository = std::make_shared<ssa::tests::presentation_smoke::FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<ssa::tests::presentation_smoke::FakeCommands>();
            ssa::presentation::MainViewModel viewModel(service, commands);
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

            auto* openAnalytics =
                mainWindow->findChild<QObject*>(QStringLiteral("openAnalyticsMenuItem"));
            auto* analyticsLoader =
                mainWindow->findChild<QObject*>(QStringLiteral("analyticsWindowLoader"));
            QVERIFY(openAnalytics != nullptr);
            QVERIFY(analyticsLoader != nullptr);
            QVERIFY(!openAnalytics->property("enabled").toBool());
            QVERIFY(!analyticsLoader->property("active").toBool());
            QVERIFY(QMetaObject::invokeMethod(mainWindow.get(), "openAnalyticsWindow"));
            QVERIFY(!analyticsLoader->property("active").toBool());
        }

        void analytics_window_reopens_the_same_instance() {
            auto repository = std::make_shared<ssa::tests::presentation_smoke::FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<ssa::tests::presentation_smoke::FakeCommands>();
            auto analyticsService = std::make_shared<ssa::application::ActivityAnalyticsService>(
                std::make_shared<PassiveAnalyticsPort>());
            ssa::presentation::MainViewModel viewModel(service, commands, nullptr, nullptr, nullptr,
                                                       nullptr, nullptr, nullptr, nullptr,
                                                       analyticsService);
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

            auto* openAnalytics =
                mainWindow->findChild<QObject*>(QStringLiteral("openAnalyticsMenuItem"));
            auto* analyticsLoader =
                mainWindow->findChild<QObject*>(QStringLiteral("analyticsWindowLoader"));
            QVERIFY(openAnalytics != nullptr);
            QVERIFY(analyticsLoader != nullptr);
            QVERIFY(openAnalytics->property("enabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(mainWindow.get(), "openAnalyticsWindow"));
            auto* firstWindow = waitForVisibleWindow(QStringLiteral("Analises de SSA"));
            QVERIFY(firstWindow != nullptr);
            QCOMPARE(analyticsLoader->property("item").value<QObject*>(), firstWindow);

            firstWindow->close();
            QTRY_VERIFY_WITH_TIMEOUT(!firstWindow->isVisible(), 1000);
            QVERIFY(QMetaObject::invokeMethod(mainWindow.get(), "openAnalyticsWindow"));
            auto* reopenedWindow = waitForVisibleWindow(QStringLiteral("Analises de SSA"));
            QVERIFY(reopenedWindow != nullptr);
            QCOMPARE(reopenedWindow, firstWindow);
            reopenedWindow->close();
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
            auto preferences = std::make_shared<ssa::tests::presentation_smoke::FakePreferences>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<ssa::tests::presentation_smoke::CapturingImportPort>(), nullptr,
                nullptr,
                std::make_shared<ssa::tests::presentation_smoke::CapturingDerivadasPort>());
            ssa::presentation::MainViewModel viewModel(service, commands, preferences, nullptr,
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
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.browse()->totalRows(), 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!viewModel.browse()->visibleColumns().empty(), 1000);

            auto* mainTable = window->findChild<QObject*>(QStringLiteral("mainSsaTable"));
            auto* popup = window->findChild<QObject*>(QStringLiteral("columnSelectorPopup"));
            QVERIFY(mainTable != nullptr);
            QVERIFY(popup != nullptr);
            auto* mainTableItem = qobject_cast<QQuickItem*>(mainTable);
            QVERIFY(mainTableItem != nullptr);
            struct HeaderMenuHandles final {
                QPointer<QQuickItem> cell;
                QPointer<QObject> menu;
            };
            const auto locateHeaderMenu = [&] {
                QVariant headerCellValue;
                HeaderMenuHandles handles;
                if (!QMetaObject::invokeMethod(mainTable, "headerCellForSmoke",
                                               Q_RETURN_ARG(QVariant, headerCellValue),
                                               Q_ARG(QVariant, QStringLiteral("numero_ssa")))) {
                    return handles;
                }
                handles.cell = qobject_cast<QQuickItem*>(qvariant_cast<QObject*>(headerCellValue));
                if (handles.cell) {
                    handles.menu =
                        qvariant_cast<QObject*>(handles.cell->property("contextMenuForSmoke"));
                }
                return handles;
            };
            const auto openHeaderMenu = [&](const HeaderMenuHandles& handles) {
                const auto center = handles.cell->mapToItem(
                    window->contentItem(),
                    QPointF{handles.cell->width() / 2.0, handles.cell->height() / 2.0});
                QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, center.toPoint());
            };
            const auto clickMenuAction = [&](QQuickItem* action) {
                const auto center = action->mapToItem(
                    window->contentItem(), QPointF{action->width() / 2.0, action->height() / 2.0});
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
            };
            auto headerHandles = locateHeaderMenu();
            QTRY_VERIFY_WITH_TIMEOUT((!(headerHandles = locateHeaderMenu()).cell.isNull() &&
                                      !headerHandles.menu.isNull()),
                                     1000);
            openHeaderMenu(headerHandles);
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu->property("visible").toBool(), 1000);

            auto* cellMenu = window->findChild<QObject*>(QStringLiteral("cellContextMenu"));
            auto* cellAction =
                window->findChild<QQuickItem*>(QStringLiteral("cellConfigureColumnsAction"));
            auto* copyCellAction = window->findChild<QQuickItem*>(QStringLiteral("copyCellAction"));
            auto* copyRowAction = window->findChild<QQuickItem*>(QStringLiteral("copyRowAction"));
            auto* copySsaAction = window->findChild<QQuickItem*>(QStringLiteral("copySsaAction"));
            auto* copyGraphAction =
                window->findChild<QQuickItem*>(QStringLiteral("copyGraphAction"));
            auto* openSamAction = window->findChild<QQuickItem*>(QStringLiteral("openSamAction"));
            auto* openDetailsAction =
                window->findChild<QQuickItem*>(QStringLiteral("openDetailsAction"));
            QPointer<QQuickItem> filterColumnAction =
                headerHandles.menu->findChild<QQuickItem*>(QStringLiteral("filterColumnAction"));
            QPointer<QQuickItem> hideColumnAction =
                headerHandles.menu->findChild<QQuickItem*>(QStringLiteral("hideColumnAction"));
            QPointer<QQuickItem> resetSortAction =
                headerHandles.menu->findChild<QQuickItem*>(QStringLiteral("resetSortAction"));
            QPointer<QQuickItem> headerConfigureColumnsAction =
                headerHandles.menu->findChild<QQuickItem*>(
                    QStringLiteral("headerConfigureColumnsAction"));
            QVERIFY(!filterColumnAction.isNull());
            QVERIFY(cellMenu != nullptr);
            QVERIFY(cellAction != nullptr);
            QVERIFY(copyCellAction != nullptr);
            QVERIFY(copyRowAction != nullptr);
            QVERIFY(copySsaAction != nullptr);
            QVERIFY(copyGraphAction != nullptr);
            QVERIFY(openSamAction != nullptr);
            QVERIFY(openDetailsAction != nullptr);
            QVERIFY(hideColumnAction != nullptr);
            QVERIFY(resetSortAction != nullptr);
            QVERIFY(headerConfigureColumnsAction != nullptr);

            QCOMPARE(copyCellAction->property("actionId").toString(), QString("copy_cell"));
            QCOMPARE(copyRowAction->property("actionId").toString(), QString("copy_row"));
            QCOMPARE(copySsaAction->property("actionId").toString(), QString("copy_ssa"));
            QCOMPARE(copyGraphAction->property("actionId").toString(), QString("copy_graph_svg"));
            QCOMPARE(openSamAction->property("actionId").toString(), QString("open_sam"));
            QCOMPARE(openDetailsAction->property("actionId").toString(), QString("open_details"));
            QCOMPARE(filterColumnAction->property("actionId").toString(), QString("filter_column"));
            QCOMPARE(hideColumnAction->property("actionId").toString(), QString("hide_column"));
            QCOMPARE(resetSortAction->property("actionId").toString(), QString("reset_sort"));
            QCOMPARE(headerConfigureColumnsAction->property("actionId").toString(),
                     QString("configure_columns_from_header"));

            const auto focusRequest = viewModel.browse()->filters()->focusColumnRequest();
            clickMenuAction(filterColumnAction);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.browse()->filters()->focusColumnRequest(),
                                      focusRequest + 1, 1000);
            QCOMPARE(viewModel.browse()->filters()->columnKey(), QString("numero_ssa"));
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu.isNull() ||
                                         !headerHandles.menu->property("visible").toBool(),
                                     1000);

            mainTable->setProperty("contextCellText", QString{});
            mainTable->setProperty("contextSsaNumber", QString{});
            QVERIFY(!copyCellAction->isEnabled());
            QVERIFY(!openDetailsAction->isEnabled());

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

            viewModel.browse()->selectRow(0);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.browse()->currentRow(), 0, 1000);

            QVariant firstCellCenterValue;
            QVERIFY(QMetaObject::invokeMethod(mainTable, "firstCellCenterForSmoke",
                                              Q_RETURN_ARG(QVariant, firstCellCenterValue)));
            const auto firstCellCenter = firstCellCenterValue.toPointF();
            QVERIFY(firstCellCenter.x() >= 0.0);
            const auto firstCellCenterInWindow =
                mainTableItem->mapToItem(window->contentItem(), firstCellCenter);
            QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                              firstCellCenterInWindow.toPoint());
            QTRY_VERIFY_WITH_TIMEOUT(cellMenu->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(copyGraphAction->isEnabled(), 1000);
            clickMenuAction(copyGraphAction);
            QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->text(),
                                      viewModel.browse()->details()->graphModel()->svg(), 1000);

            const auto commandCount = commands->commands().size();
            QVERIFY(QMetaObject::invokeMethod(openSamAction, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), commandCount + 1, 1000);
            QCOMPARE(commands->commands().back().kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(
                QString::fromStdString(commands->commands().back().parameters.at("ssa_number")),
                QStringLiteral("202500001"));

            QVERIFY(QMetaObject::invokeMethod(openDetailsAction, "triggered"));
            auto* detailsWindow = waitForVisibleWindow(QStringLiteral("Detalhes da SSA"));
            QVERIFY(detailsWindow != nullptr);
            detailsWindow->close();

            viewModel.browse()->sortByColumn(1);
            QTRY_VERIFY_WITH_TIMEOUT(!viewModel.browse()->sortColumnKey().isEmpty(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT((!(headerHandles = locateHeaderMenu()).cell.isNull() &&
                                      !headerHandles.menu.isNull()),
                                     1000);
            resetSortAction =
                headerHandles.menu->findChild<QQuickItem*>(QStringLiteral("resetSortAction"));
            QVERIFY(!resetSortAction.isNull());
            QVERIFY(QMetaObject::invokeMethod(headerHandles.menu, "popup"));
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu->property("visible").toBool(), 1000);
            clickMenuAction(resetSortAction);
            QTRY_VERIFY_WITH_TIMEOUT(viewModel.browse()->sortColumnKey().isEmpty(), 1000);
            auto* preferenceFlow = qobject_cast<ssa::presentation::MainPreferenceFlowCoordinator*>(
                viewModel.preferenceFlow());
            QVERIFY(preferenceFlow != nullptr);
            auto* preferencesCoordinator =
                viewModel.findChild<ssa::presentation::UserPreferencesCoordinator*>();
            QVERIFY(preferencesCoordinator != nullptr);
            preferenceFlow->saveNowOrSchedule();
            QTRY_VERIFY_WITH_TIMEOUT(!preferencesCoordinator->running(), 1000);
            const auto saveCountBeforeHide = preferences->saveCount();

            QTRY_VERIFY_WITH_TIMEOUT((!(headerHandles = locateHeaderMenu()).cell.isNull() &&
                                      !headerHandles.menu.isNull()),
                                     1000);
            headerConfigureColumnsAction = headerHandles.menu->findChild<QQuickItem*>(
                QStringLiteral("headerConfigureColumnsAction"));
            QVERIFY(!headerConfigureColumnsAction.isNull());
            QVERIFY(QMetaObject::invokeMethod(headerHandles.menu, "popup"));
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu->property("visible").toBool(), 1000);
            clickMenuAction(headerConfigureColumnsAction);
            QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu.isNull() ||
                                         !headerHandles.menu->property("visible").toBool(),
                                     1000);
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));

            const auto visibleColumnsBeforeHide = viewModel.browse()->visibleColumns().size();
            QTRY_VERIFY_WITH_TIMEOUT((!(headerHandles = locateHeaderMenu()).cell.isNull() &&
                                      !headerHandles.menu.isNull()),
                                     1000);
            hideColumnAction =
                headerHandles.menu->findChild<QQuickItem*>(QStringLiteral("hideColumnAction"));
            QVERIFY(!hideColumnAction.isNull());
            QVERIFY(QMetaObject::invokeMethod(headerHandles.menu, "popup"));
            QTRY_VERIFY_WITH_TIMEOUT(headerHandles.menu->property("visible").toBool(), 1000);
            clickMenuAction(hideColumnAction);
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.browse()->visibleColumns().size(),
                                      visibleColumnsBeforeHide - 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), saveCountBeforeHide + 1, 1000);

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
            auto preferences = std::make_shared<ssa::tests::presentation_smoke::FakePreferences>();
            auto importPort =
                std::make_shared<ssa::tests::presentation_smoke::CapturingImportPort>();
            auto derivadasPort =
                std::make_shared<ssa::tests::presentation_smoke::CapturingDerivadasPort>();
            derivadasPort->setLegacySpreadsheetConverterAvailable(false);
            auto maintenancePort =
                std::make_shared<ssa::tests::presentation_smoke::CapturingMaintenancePort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                importPort, nullptr, maintenancePort, derivadasPort);
            ssa::presentation::MainViewModel viewModel(service, commands, preferences, nullptr,
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
            auto* quickWindow = qobject_cast<QQuickWindow*>(mainWindow.get());
            QVERIFY(quickWindow != nullptr);
            quickWindow->show();
            QTRY_VERIFY_WITH_TIMEOUT(quickWindow->isExposed(), 1000);
            const auto menuBarItemFor = [&](const QString& text) {
                for (auto* item : quickWindow->findChildren<QQuickItem*>()) {
                    if (item->isVisible() && item->property("text").toString() == text &&
                        item->y() < quickWindow->height() / 2.0) {
                        return item;
                    }
                }
                return static_cast<QQuickItem*>(nullptr);
            };
            const auto clickItem = [&](QQuickItem& item) {
                const auto center = item.mapToItem(
                    quickWindow->contentItem(), QPointF{item.width() / 2.0, item.height() / 2.0});
                QTest::mouseClick(quickWindow, Qt::LeftButton, Qt::NoModifier, center.toPoint());
            };

            viewModel.logs()->append(
                QStringLiteral("Error"), QStringLiteral("Workflow"),
                QStringLiteral("Falha de importacao"),
                QStringLiteral("invalid integer value for column numero_desvios"));
            auto* helpMenu = mainWindow->findChild<QObject*>(QStringLiteral("helpMenu"));
            auto* openLogHistory =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("openLogHistoryMenuItem"));
            QVERIFY(helpMenu != nullptr);
            QVERIFY(openLogHistory != nullptr);
            QVERIFY(QMetaObject::invokeMethod(helpMenu, "popup"));
            QTRY_VERIFY_WITH_TIMEOUT(openLogHistory->isVisible(), 1000);
            const auto logActionCenter = openLogHistory->mapToItem(
                quickWindow->contentItem(),
                QPointF{openLogHistory->width() / 2.0, openLogHistory->height() / 2.0});
            QTest::mouseClick(quickWindow, Qt::LeftButton, Qt::NoModifier,
                              logActionCenter.toPoint());

            auto* logWindow = waitForVisibleWindow(QStringLiteral("Historico de logs e erros"));
            QVERIFY(logWindow != nullptr);
            auto* logDetail = logWindow->findChild<QQuickItem*>(QStringLiteral("logHistoryDetail"));
            auto* copySelected =
                logWindow->findChild<QQuickItem*>(QStringLiteral("copySelectedLogButton"));
            QVERIFY(logDetail != nullptr);
            QVERIFY(copySelected != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(
                logDetail->property("text").toString().contains(QStringLiteral("numero_desvios")),
                1000);
            const auto copyCenter = copySelected->mapToItem(
                qobject_cast<QQuickWindow*>(logWindow)->contentItem(),
                QPointF{copySelected->width() / 2.0, copySelected->height() / 2.0});
            QTest::mouseClick(logWindow, Qt::LeftButton, Qt::NoModifier, copyCenter.toPoint());
            QTRY_VERIFY_WITH_TIMEOUT(
                QGuiApplication::clipboard()->text().contains(QStringLiteral("numero_desvios")),
                1000);
            const auto logImage = qobject_cast<QQuickWindow*>(logWindow)->grabWindow();
            QVERIFY(!logImage.isNull());
            QVERIFY(logImage.save(QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("log-history-dialog.png"))));
            logWindow->close();

            auto* statusText =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("statusMessageText"));
            QVERIFY(statusText != nullptr);
            QVERIFY(statusText->property("readOnly").toBool());
            QVERIFY(statusText->property("selectByMouse").toBool());

            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openHelpDialog",
                                                QStringLiteral("Ajuda")));
            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openAboutDialog",
                                                QStringLiteral("Sobre")));
            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openSamRefreshDialog",
                                                QStringLiteral("Atualizacao SAM")));

            auto* openDatabase =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("openDatabaseMenuItem"));
            QVERIFY(openDatabase != nullptr);
            auto* fileMenuBarItem = menuBarItemFor(QStringLiteral("Arquivo"));
            QVERIFY(fileMenuBarItem != nullptr);
            clickItem(*fileMenuBarItem);
            QTRY_VERIFY_WITH_TIMEOUT(openDatabase->isVisible(), 1000);
            clickItem(*openDatabase);
            auto* databaseDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("databaseFileDialog"));
            QVERIFY(databaseDialog != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(databaseDialog->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(databaseDialog, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!databaseDialog->property("visible").toBool(), 1000);

            auto* openImportData =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("openImportDataMenuItem"));
            QVERIFY(openImportData != nullptr);
            auto* importMenuBarItem = menuBarItemFor(QStringLiteral("Importacao"));
            QVERIFY(importMenuBarItem != nullptr);
            clickItem(*importMenuBarItem);
            QTRY_VERIFY_WITH_TIMEOUT(openImportData->isVisible(), 1000);
            clickItem(*openImportData);
            auto* importDataDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("importDataFileDialog"));
            QVERIFY(importDataDialog != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(importDataDialog->property("visible").toBool(), 1000);
            QCOMPARE(importDataDialog->property("title").toString(),
                     QStringLiteral("Importar XLSX externo"));
            QCOMPARE(importDataDialog->property("nameFilters").toStringList(),
                     QStringList{QStringLiteral("Planilhas XLSX (*.xlsx)")});
            QVERIFY(QMetaObject::invokeMethod(importDataDialog, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!importDataDialog->property("visible").toBool(), 1000);

            auto* exportResults =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("exportResultsMenuItem"));
            auto* rescanIncremental =
                mainWindow->findChild<QObject*>(QStringLiteral("rescanIncrementalMenuItem"));
            auto* rescanFull =
                mainWindow->findChild<QObject*>(QStringLiteral("rescanFullMenuItem"));
            auto* cleanOrphans =
                mainWindow->findChild<QObject*>(QStringLiteral("cleanOrphanDerivationsMenuItem"));
            auto* applyFilters =
                mainWindow->findChild<QObject*>(QStringLiteral("applyFiltersMenuItem"));
            auto* exportFilters =
                mainWindow->findChild<QObject*>(QStringLiteral("exportFiltersMenuItem"));
            auto* importFilters =
                mainWindow->findChild<QObject*>(QStringLiteral("importFiltersMenuItem"));
            auto* savePreferences =
                mainWindow->findChild<QObject*>(QStringLiteral("savePreferencesMenuItem"));
            auto* toggleDetails =
                mainWindow->findChild<QObject*>(QStringLiteral("toggleDetailsMenuItem"));
            auto* compactDatabase =
                mainWindow->findChild<QQuickItem*>(QStringLiteral("compactDatabaseMenuItem"));
            auto* cancelAll = mainWindow->findChild<QObject*>(QStringLiteral("cancelAllMenuItem"));
            auto* installationGuide =
                mainWindow->findChild<QObject*>(QStringLiteral("installationGuideMenuItem"));
            auto* exportResultsDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("exportResultsFileDialog"));
            auto* exportFiltersDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("exportFiltersFileDialog"));
            auto* importFiltersDialog =
                mainWindow->findChild<QObject*>(QStringLiteral("importFiltersFileDialog"));
            QVERIFY(exportResults != nullptr);
            QVERIFY(rescanIncremental != nullptr);
            QVERIFY(rescanFull != nullptr);
            QVERIFY(cleanOrphans != nullptr);
            QVERIFY(applyFilters != nullptr);
            QVERIFY(exportFilters != nullptr);
            QVERIFY(importFilters != nullptr);
            QVERIFY(savePreferences != nullptr);
            QVERIFY(toggleDetails != nullptr);
            QVERIFY(compactDatabase != nullptr);
            QVERIFY(cancelAll != nullptr);
            QVERIFY(installationGuide != nullptr);
            QVERIFY(exportResultsDialog != nullptr);
            QVERIFY(exportFiltersDialog != nullptr);
            QVERIFY(importFiltersDialog != nullptr);

            fileMenuBarItem = menuBarItemFor(QStringLiteral("Arquivo"));
            QVERIFY(fileMenuBarItem != nullptr);
            clickItem(*fileMenuBarItem);
            QTRY_VERIFY_WITH_TIMEOUT(exportResults->isVisible(), 1000);
            clickItem(*exportResults);
            QTRY_VERIFY_WITH_TIMEOUT(exportResultsDialog->property("visible").toBool(), 1000);
            QCOMPARE(exportResultsDialog->property("title").toString(),
                     QStringLiteral("Exportar CSV"));
            QVERIFY(QMetaObject::invokeMethod(exportResultsDialog, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!exportResultsDialog->property("visible").toBool(), 1000);

            QVERIFY(QMetaObject::invokeMethod(exportFilters, "triggered"));
            QTRY_VERIFY_WITH_TIMEOUT(exportFiltersDialog->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(exportFiltersDialog, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!exportFiltersDialog->property("visible").toBool(), 1000);

            QVERIFY(QMetaObject::invokeMethod(importFilters, "triggered"));
            QTRY_VERIFY_WITH_TIMEOUT(importFiltersDialog->property("visible").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(importFiltersDialog, "close"));
            QTRY_VERIFY_WITH_TIMEOUT(!importFiltersDialog->property("visible").toBool(), 1000);

            QVERIFY(QMetaObject::invokeMethod(rescanIncremental, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().front().mode, ssa::ports::RescanMode::Incremental);
            QTRY_VERIFY_WITH_TIMEOUT(
                !viewModel.actions()->workflows()->property("running").toBool(), 1000);

            QVERIFY(QMetaObject::invokeMethod(rescanFull, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Full);
            QTRY_VERIFY_WITH_TIMEOUT(
                !viewModel.actions()->workflows()->property("running").toBool(), 1000);

            QVERIFY(QMetaObject::invokeMethod(cleanOrphans, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(derivadasPort->syncCalls(), std::size_t{1}, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(
                !viewModel.actions()->workflows()->property("running").toBool(), 1000);

            auto* maintenanceMenuBarItem = menuBarItemFor(QStringLiteral("Manutencao"));
            QVERIFY(maintenanceMenuBarItem != nullptr);
            clickItem(*maintenanceMenuBarItem);
            QTRY_VERIFY_WITH_TIMEOUT(compactDatabase->isVisible(), 1000);
            clickItem(*compactDatabase);
            QTRY_COMPARE_WITH_TIMEOUT(maintenancePort->vacuumAnalyzeCalls(), std::size_t{1}, 1000);

            const bool initialDetailsVisible = viewModel.ui()->detailsVisible();
            toggleDetails->setProperty("checked", !initialDetailsVisible);
            QVERIFY(QMetaObject::invokeMethod(toggleDetails, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(viewModel.ui()->detailsVisible(), !initialDetailsVisible,
                                      1000);

            QVERIFY(QMetaObject::invokeMethod(savePreferences, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);

            QVERIFY(QMetaObject::invokeMethod(installationGuide, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind,
                     ssa::ports::ExternalCommandKind::OpenInstallationGuide);

            const auto requestsBeforeApply = repository->requests().size();
            viewModel.browse()->search()->setText(QStringLiteral("menu filter"));
            QVERIFY(QMetaObject::invokeMethod(applyFilters, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), requestsBeforeApply + 1, 1000);
            QCOMPARE(QString::fromStdString(repository->requests().back().searchText),
                     QStringLiteral("menu filter"));

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
            const auto statusCancelScreenshotError =
                captureDialogScreenshot(*mainWindow, QStringLiteral("status-cancel.png"));
            QVERIFY2(statusCancelScreenshotError.isEmpty(),
                     qPrintable(statusCancelScreenshotError));

            QTRY_VERIFY_WITH_TIMEOUT(cancelAll->property("enabled").toBool(), 1000);
            QVERIFY(QMetaObject::invokeMethod(cancelAll, "triggered"));
            QTRY_COMPARE_WITH_TIMEOUT(cancelButton->property("text").toString(),
                                      QStringLiteral("Cancelando..."), 1000);
            const auto statusCancelingScreenshotError =
                captureDialogScreenshot(*mainWindow, QStringLiteral("status-canceling.png"));
            QVERIFY2(statusCancelingScreenshotError.isEmpty(),
                     qPrintable(statusCancelingScreenshotError));

            QVERIFY(QMetaObject::invokeMethod(forceShutdownDialog, "open"));
            QTRY_VERIFY_WITH_TIMEOUT(forceShutdownDialog->property("visible").toBool(), 1000);
            const auto forceShutdownScreenshotError =
                captureDialogScreenshot(*mainWindow, QStringLiteral("force-shutdown.png"));
            QVERIFY2(forceShutdownScreenshotError.isEmpty(),
                     qPrintable(forceShutdownScreenshotError));
            QVERIFY(QMetaObject::invokeMethod(forceShutdownDialog, "close"));
            viewModel.browse()->status()->setLoading(false);
        }
    };

} // namespace

QTEST_MAIN(HelpAboutQmlTest)

#include "HelpAboutQmlTest.moc"
