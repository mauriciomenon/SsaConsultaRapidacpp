#include "SsaCliController.h"

#include "SsaCliDatabasePath.h"
#include "SsaCliImportPaths.h"
#include "SsaCliPrinter.h"
#include "SsaCliRequestMapper.h"
#include "SsaCliWorkflowRunner.h"
#include "application/SsaBrowseService.h"
#include "domain/ColumnCatalog.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QStringList>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    void configureParser(QCommandLineParser& parser) {
        parser.setApplicationDescription("SSA Consulta Rapida CLI");
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addOption(QCommandLineOption(QStringList{"db"}, "SQLite database path.", "path"));
        parser.addOption(
            QCommandLineOption(QStringList{"docs-dir"}, "Input XLSX directory.", "path"));
        parser.addOption(QCommandLineOption(QStringList{"search"}, "General search text.", "text"));
        parser.addOption(
            QCommandLineOption(QStringList{"details"}, "Print one SSA by number.", "ssa"));
        parser.addOption(
            QCommandLineOption(QStringList{"export"}, "Export filtered list to CSV.", "path"));
        parser.addOption(
            QCommandLineOption(QStringList{"columns"}, "Comma-separated column keys.", "keys"));
        parser.addOption(QCommandLineOption(QStringList{"page"}, "1-based page number.", "number"));
        parser.addOption(QCommandLineOption(QStringList{"page-size"}, "Rows per page.", "number"));
        parser.addOption(QCommandLineOption(QStringList{"sort"}, "Sort column key.", "key"));
        parser.addOption(QCommandLineOption(QStringList{"asc"}, "Sort ascending."));
        parser.addOption(QCommandLineOption(QStringList{"desc"}, "Sort descending."));
        parser.addOption(QCommandLineOption(QStringList{"include-sca-ses-ste"},
                                            "Include SCA/SES/STE rows; excluded by default."));
        parser.addOption(
            QCommandLineOption(QStringList{"rescan"}, "Compatibility alias for full rescan."));
        parser.addOption(QCommandLineOption(QStringList{"force-rescan"}, "Run full rescan."));
        parser.addOption(
            QCommandLineOption(QStringList{"incremental-rescan"}, "Run incremental rescan."));
        parser.addOption(QCommandLineOption(QStringList{"skip-import"},
                                            "Deprecated compatibility flag; accepted as no-op."));
        parser.addOption(
            QCommandLineOption(QStringList{"optimized"}, "Use optimized import strategy."));
        parser.addOption(
            QCommandLineOption(QStringList{"standard"}, "Use standard import strategy."));
        parser.addOption(QCommandLineOption(QStringList{"reset-db"}, "Reset the local database."));
        parser.addOption(QCommandLineOption(QStringList{"clean-data"}, "Clean imported data."));
        parser.addOption(QCommandLineOption(QStringList{"vacuum-analyze"},
                                            "Run SQLite vacuum/analyze maintenance."));
        parser.addOption(
            QCommandLineOption(QStringList{"sync-derivadas"}, "Synchronize derivadas data."));
        parser.addOption(
            QCommandLineOption(QStringList{"log-level"}, "Set logging level.", "level"));
    }

    int parseLogLevel(const QString& level) {
        const auto normalized = level.toLower();
        if (normalized == "trace" || normalized == "debug") {
            QLoggingCategory::setFilterRules("*.debug=true\n"
                                             "*.info=true\n"
                                             "*.warning=true\n"
                                             "*.critical=true\n");
            return 0;
        }
        if (normalized == "info") {
            QLoggingCategory::setFilterRules("*.debug=false\n"
                                             "*.info=true\n"
                                             "*.warning=true\n"
                                             "*.critical=true\n");
            return 0;
        }
        if (normalized == "warning") {
            QLoggingCategory::setFilterRules("*.debug=false\n"
                                             "*.info=false\n"
                                             "*.warning=true\n"
                                             "*.critical=true\n");
            return 0;
        }
        if (normalized == "error" || normalized == "critical") {
            QLoggingCategory::setFilterRules("*.debug=false\n"
                                             "*.info=false\n"
                                             "*.warning=false\n"
                                             "*.critical=true\n");
            return 0;
        }
        if (normalized == "off") {
            QLoggingCategory::setFilterRules("*.debug=false\n"
                                             "*.info=false\n"
                                             "*.warning=false\n"
                                             "*.critical=false\n");
            return 0;
        }
        std::cerr << "error: invalid --log-level value, expected: trace|debug|info|warning|error|critical|off\n";
        return 2;
    }

} // namespace

namespace ssa::app::cli {

    SsaCliController::SsaCliController(BrowseFactory browseFactory, WorkflowFactory workflowFactory,
                                       DatabaseWorkflowFactory databaseWorkflowFactory)
        : browseFactory_(std::move(browseFactory)), workflowFactory_(std::move(workflowFactory)),
          databaseWorkflowFactory_(std::move(databaseWorkflowFactory)) {
        if (!browseFactory_) {
            throw std::invalid_argument("browse factory is required");
        }
        if (!workflowFactory_) {
            throw std::invalid_argument("workflow factory is required");
        }
        if (!databaseWorkflowFactory_) {
            throw std::invalid_argument("database workflow factory is required");
        }
    }

    int SsaCliController::run(const QStringList& arguments) const {
        QCommandLineParser parser;
        configureParser(parser);
        if (!parser.parse(arguments)) {
            std::cerr << "error: " << parser.errorText().toStdString() << '\n';
            return 2;
        }
        if (parser.isSet("log-level")) {
            const auto exitCode = parseLogLevel(parser.value("log-level"));
            if (exitCode != 0) {
                return exitCode;
            }
        }
        if (parser.isSet("help")) {
            std::cout << parser.helpText().toStdString();
            return 0;
        }
        if (parser.isSet("version")) {
            std::cout << QCoreApplication::applicationName().toStdString() << ' '
                      << QCoreApplication::applicationVersion().toStdString() << '\n';
            return 0;
        }

        try {
            if (SsaCliWorkflowRunner::hasWorkflowCommand(parser)) {
                std::shared_ptr<application::SsaWorkflowService> workflows;
                if (SsaCliWorkflowRunner::requiresDatabase(parser)) {
                    if (!parser.isSet("db")) {
                        std::cerr << "missing required --db\n";
                        return 2;
                    }
                    const auto databasePath = SsaCliDatabasePath::required(parser);
                    const auto docsDir = SsaCliImportPaths::docsDirectory(parser, databasePath);
                    if (SsaCliImportPaths::isRescanOperation(parser) &&
                        (docsDir.empty() || !std::filesystem::exists(docsDir))) {
                        std::cerr << "missing --docs-dir and no docs_entrada directory was found\n";
                        return 2;
                    }
                    workflows = databaseWorkflowFactory_(databasePath, docsDir);
                } else {
                    workflows = workflowFactory_();
                }
                if (!workflows) {
                    throw std::runtime_error("workflow service factory returned null");
                }
                return runWorkflow(parser, *workflows);
            }
            if (!parser.isSet("db")) {
                std::cerr << "missing required --db\n";
                return 2;
            }
            if (parser.isSet("export")) {
                return runExport(parser, SsaCliDatabasePath::required(parser));
            }
            const auto browse = createBrowseService(parser);
            if (!browse) {
                throw std::runtime_error("browse service factory returned null");
            }
            if (parser.isSet("details")) {
                return runDetails(parser, *browse);
            }
            return runPage(parser, *browse);
        } catch (const std::exception& exc) {
            std::cerr << "error: " << exc.what() << '\n';
            return 1;
        }
    }

    std::shared_ptr<application::SsaBrowseService>
    SsaCliController::createBrowseService(const QCommandLineParser& parser) const {
        return browseFactory_(SsaCliDatabasePath::required(parser));
    }

    int SsaCliController::runDetails(const QCommandLineParser& parser,
                                     const application::SsaBrowseService& browse) const {
        const auto record = browse.details(parser.value("details").toStdString());
        if (!record) {
            std::cerr << "ssa not found\n";
            return 1;
        }
        SsaCliPrinter{std::cout}.printDetails(*record);
        return 0;
    }

    int SsaCliController::runPage(const QCommandLineParser& parser,
                                  const application::SsaBrowseService& browse) const {
        const auto requestedColumns = SsaCliRequestMapper::requestedColumns(parser);
        const auto outputColumns = browse.columnsOrDefault(requestedColumns);
        const auto request = SsaCliRequestMapper::pageRequest(parser, outputColumns);

        const auto page = browse.page(request);
        SsaCliPrinter{std::cout}.printPage(page, outputColumns);
        return 0;
    }

    int SsaCliController::runWorkflow(const QCommandLineParser& parser,
                                      const application::SsaWorkflowService& workflows) const {
        ports::WorkflowResult result;
        result = SsaCliWorkflowRunner::runSelected(parser, workflows);
        std::cerr << result.message << '\n';
        if (result.status == ports::WorkflowStatus::NotImplemented) {
            return 3;
        }
        return result.ok() ? 0 : 1;
    }

    int SsaCliController::runExport(const QCommandLineParser& parser,
                                    const std::filesystem::path& databasePath) const {
        const auto requestedColumns = SsaCliRequestMapper::requestedColumns(parser);
        const auto browse = browseFactory_(databasePath);
        if (!browse) {
            throw std::runtime_error("browse service factory returned null");
        }
        const auto outputColumns = browse->columnsOrDefault(requestedColumns);
        const auto workflows = databaseWorkflowFactory_(
            databasePath, SsaCliImportPaths::docsDirectory(parser, databasePath));

        ports::ExportFilteredListRequest request;
        request.query = SsaCliRequestMapper::pageRequest(parser, outputColumns);
        request.outputPath = std::filesystem::path{parser.value("export").toStdString()};
        const auto result = workflows->exportFilteredList(request);
        std::cerr << result.message << '\n';
        return result.ok() ? 0 : 1;
    }

} // namespace ssa::app::cli
