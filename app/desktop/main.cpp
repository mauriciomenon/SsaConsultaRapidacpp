#include "DesktopSmokeCapture.h"
#include "application/SsaWorkflowService.h"
#include "application/UnavailableWorkflowPort.h"
#include "infra/export/CsvExportPort.h"
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
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QVariant>

#include <exception>
#include <filesystem>
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
    parser.addOption(QCommandLineOption(QStringList{"smoke-exit-ms"},
                                        "Exit automatically after N milliseconds.", "ms"));
    parser.addOption(QCommandLineOption(QStringList{"screenshot"},
                                        "Write the startup window screenshot and exit.", "path"));
    parser.addOption(QCommandLineOption(QStringList{"open-preferences"},
                                        "Open preferences before screenshot smoke capture."));
    parser.process(app);

    ssa::platform::StartupOptions options;
    try {
        options = ssa::platform::StartupOptions::fromParser(parser);
    } catch (const std::exception& exc) {
        std::cerr << "startup error: " << exc.what() << '\n';
        return 2;
    }
    const ssa::platform::AppPaths paths(options.projectRoot, options.configDir);
    try {
        paths.ensureConfigDirectory();
    } catch (const std::exception& exc) {
        std::cerr << "startup error: " << exc.what() << '\n';
        return 2;
    }
    const auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(
        std::filesystem::path{options.databasePath.toStdString()});
    const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
    const auto exportPort = std::make_shared<ssa::infra::exporting::CsvExportPort>(repository);
    const auto unavailableWorkflow = std::make_shared<ssa::application::UnavailableWorkflowPort>();
    const auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        unavailableWorkflow, exportPort, unavailableWorkflow, unavailableWorkflow);
    ssa::platform::LocalOpenPaths commandPaths;
    commandPaths.inputFolder = paths.inputFolderPath();
    commandPaths.processedFolder = paths.processedFolderPath();
    commandPaths.redundantFolder = paths.redundantFolderPath();
    commandPaths.installationGuide = paths.installationGuidePath();
    const auto commands = std::make_shared<ssa::platform::DesktopExternalCommandPort>(
        QUrl{"https://sam.itaipu.gov.br"}, commandPaths,
        std::vector<std::filesystem::path>{paths.projectRootPath()});
    const auto preferences = std::make_shared<ssa::infra::preferences::JsonUserPreferencesStore>(
        std::filesystem::path{paths.preferencesFile().toStdString()});
    ssa::presentation::MainViewModel mainViewModel(service, commands, preferences, workflows);
    ssa::app::desktop::DesktopSmokeController smokeController;

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"mainViewModel", QVariant::fromValue(&mainViewModel)},
                                 {"smokeController", QVariant::fromValue(&smokeController)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SsaConsultaRapida", "Main");
    ssa::app::desktop::DesktopSmokeCapture::installIfRequested(parser, engine, smokeController);
    if (parser.isSet("smoke-exit-ms")) {
        bool ok = false;
        const int delayMs = parser.value("smoke-exit-ms").toInt(&ok);
        if (ok && delayMs > 0) {
            QTimer::singleShot(delayMs, &app, &QCoreApplication::quit);
        }
    }
    return app.exec();
}
