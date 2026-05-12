#include "infra/preferences/JsonUserPreferencesStore.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "platform/AppPaths.h"
#include "platform/DesktopExternalCommandPort.h"
#include "platform/StartupOptions.h"
#include "presentation/MainViewModel.h"
#include "query/SsaQueryService.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QVariant>

#include <filesystem>
#include <memory>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("SSA Consulta Rapida");
    QGuiApplication::setOrganizationName("Menon");
    QFont uiFont;
    uiFont.setFamilies({"Arial", "Helvetica Neue", "DejaVu Sans", "Noto Sans", "Liberation Sans"});
    QGuiApplication::setFont(uiFont);
    QQuickStyle::setStyle("Fusion");

    QCommandLineParser parser;
    parser.setApplicationDescription("SSA Consulta Rapida C++/QML");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList{"project-root"},
                                        "Project root used for default paths.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"db"}, "SQLite database path.", "path"));
    parser.addOption(
        QCommandLineOption(QStringList{"config-dir"}, "Configuration directory.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"smoke-exit-ms"},
                                        "Exit automatically after N milliseconds.", "ms"));
    parser.addOption(QCommandLineOption(QStringList{"screenshot"},
                                        "Write the startup window screenshot and exit.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"open-preferences"},
                                        "Open preferences before screenshot smoke capture."));
    parser.process(app);

    const auto options = ssa::platform::StartupOptions::fromParser(parser);
    const ssa::platform::AppPaths paths(options.projectRoot, options.configDir);
    const auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(
        std::filesystem::path{options.databasePath.toStdString()});
    const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
    const auto commands = std::make_shared<ssa::platform::DesktopExternalCommandPort>();
    const auto preferences = std::make_shared<ssa::infra::preferences::JsonUserPreferencesStore>(
        std::filesystem::path{paths.preferencesFile().toStdString()});
    ssa::presentation::MainViewModel mainViewModel(service, commands, preferences);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"mainViewModel", QVariant::fromValue(&mainViewModel)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SsaConsultaRapida", "Main");
    if (parser.isSet("screenshot")) {
        const QString screenshotPath = parser.value("screenshot");
        const bool openPreferences = parser.isSet("open-preferences");
        QTimer::singleShot(1200, &app, [&engine, screenshotPath, openPreferences] {
            if (engine.rootObjects().isEmpty()) {
                QCoreApplication::exit(2);
                return;
            }
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            if (window == nullptr) {
                QCoreApplication::exit(2);
                return;
            }
            if (openPreferences) {
                QMetaObject::invokeMethod(window, "openPreferencesForSmoke");
            }
            const QImage image = window->grabWindow();
            if (image.isNull() || !image.save(screenshotPath)) {
                QCoreApplication::exit(2);
                return;
            }
            QCoreApplication::quit();
        });
    }
    if (parser.isSet("smoke-exit-ms")) {
        bool ok = false;
        const int delayMs = parser.value("smoke-exit-ms").toInt(&ok);
        if (ok && delayMs > 0) {
            QTimer::singleShot(delayMs, &app, &QCoreApplication::quit);
        }
    }
    return app.exec();
}
