#include "DesktopApplicationRuntime.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

#include <exception>
#include <iostream>
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
    parser.addOption(QCommandLineOption(QStringList{"config-dir"},
                                        "Configuration directory used for the preferences file.",
                                        "path"));
    parser.addOption(QCommandLineOption(QStringList{"sam-url"},
                                        "SAM base URL used by external open commands.", "url"));
    parser.addOption(QCommandLineOption(
        QStringList{"smoke-exit-ms"}, "Exit smoke run automatically after N milliseconds.", "ms"));
    parser.addOption(QCommandLineOption(QStringList{"screenshot"},
                                        "Write the startup window screenshot and exit.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"screenshot-delay-ms"},
                                        "Delay screenshot capture after a rendered frame.", "ms"));
    parser.addOption(QCommandLineOption(QStringList{"open-preferences"},
                                        "Open preferences before screenshot smoke capture."));
    parser.addOption(QCommandLineOption(QStringList{"open-advanced-filters"},
                                        "Open advanced filters before screenshot smoke capture."));
    parser.process(app);

    std::unique_ptr<ssa::app::desktop::DesktopApplicationRuntime> runtime;
    try {
        runtime = std::make_unique<ssa::app::desktop::DesktopApplicationRuntime>(parser);
    } catch (const std::exception& exc) {
        std::cerr << "startup error: " << exc.what() << '\n';
        return 2;
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    runtime->loadMainWindow(engine);
    runtime->installSmokeCapture(parser, engine);
    if (parser.isSet("smoke-exit-ms")) {
        bool ok = false;
        const int delayMs = parser.value("smoke-exit-ms").toInt(&ok);
        if (!ok || delayMs <= 0) {
            std::cerr << "startup error: --smoke-exit-ms must be a positive integer\n";
            return 2;
        }
        QTimer::singleShot(delayMs, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
