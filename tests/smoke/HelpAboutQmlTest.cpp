#include "PresentationSmokeFakes.h"

#include "presentation/MainViewModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
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

        void main_window_opens_closes_and_reopens_help_and_about() {
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

            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openHelpDialog",
                                                QStringLiteral("Ajuda")));
            QVERIFY(dialogCanOpenCloseAndReopen(*mainWindow, "openAboutDialog",
                                                QStringLiteral("Sobre")));
        }
    };

} // namespace

QTEST_MAIN(HelpAboutQmlTest)

#include "HelpAboutQmlTest.moc"
