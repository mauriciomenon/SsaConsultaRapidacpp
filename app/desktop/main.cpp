#include "DesktopApplicationRuntime.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QtGlobal>

#include <exception>
#include <iostream>
#include <memory>

#ifndef SSA_PROJECT_VERSION
#define SSA_PROJECT_VERSION "0.0.0"
#endif

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("SSA Consulta Rapida");
    QGuiApplication::setApplicationVersion(QString::fromUtf8(SSA_PROJECT_VERSION));
    QGuiApplication::setOrganizationName("Menon");
    QFont uiFont;
#if defined(Q_OS_MACOS)
    uiFont.setFamilies({"Arial", "Helvetica Neue", "Helvetica"});
#elif defined(Q_OS_WIN)
    uiFont.setFamilies({"Segoe UI", "Arial"});
#else
    uiFont.setFamilies({"DejaVu Sans", "Noto Sans", "Liberation Sans", "Arial"});
#endif
    QGuiApplication::setFont(uiFont);
    QQuickStyle::setStyle("Fusion");

    QCommandLineParser parser;
    parser.setApplicationDescription("SSA Consulta Rapida C++/QML");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringList{"project-root"},
                                        "Project root used for default paths.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"db", "database", "database-path"},
                                        "SQLite database path.", "path"));
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
    parser.addOption(QCommandLineOption(QStringList{"smoke-advanced-popup"},
                                        "Open the advanced value popup with smoke data."));
    parser.addOption(QCommandLineOption(QStringList{"smoke-window-width"},
                                        "Set the root window width for smoke capture.", "pixels"));
    parser.addOption(QCommandLineOption(QStringList{"smoke-window-height"},
                                        "Set the root window height for smoke capture.", "pixels"));
    parser.addOption(QCommandLineOption(QStringList{"smoke-layout"},
                                        "Validate root layout bounds before smoke capture."));
    parser.addOption(
        QCommandLineOption(QStringList{"open-details-window"},
                           "Open details graph window before screenshot smoke capture."));
    parser.process(app);

    if (!parser.isSet("project-root")) {
        const auto userDataRoot = ssa::platform::StartupOptions::defaultUserDataRoot();
        if (!QDir{}.mkpath(userDataRoot)) {
            std::cerr << "startup error: cannot create user data directory: "
                      << userDataRoot.toStdString() << '\n';
            return 2;
        }
        const auto dataDirectory = QDir(userDataRoot).filePath("data");
        if (!QDir{}.mkpath(dataDirectory)) {
            std::cerr << "startup error: cannot create data directory: "
                      << dataDirectory.toStdString() << '\n';
            return 2;
        }
    }

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
    try {
        runtime->loadMainWindow(engine);
        runtime->installSmokeCapture(parser, engine);
    } catch (const std::exception& exc) {
        std::cerr << "startup error: " << exc.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "startup error: unknown exception while loading desktop runtime\n";
        return 2;
    }
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
