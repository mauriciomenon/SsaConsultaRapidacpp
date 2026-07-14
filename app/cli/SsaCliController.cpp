#include "SsaCliController.h"

#include "SsaCliDatabasePath.h"
#include "SsaCliImportPaths.h"
#include "SsaCliPrinter.h"
#include "SsaCliRequestMapper.h"
#include "SsaCliWorkflowRunner.h"
#include "application/SsaBrowseService.h"
#include "domain/ColumnCatalog.h"
#include "qt/FilesystemPath.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QStringList>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
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
        parser.addOption(QCommandLineOption(QStringList{"columns", "cols"},
                                            "Comma-separated column keys.", "keys"));
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
        parser.addOption(QCommandLineOption(QStringList{"reset-db"}, "Reset the local database."));
        parser.addOption(QCommandLineOption(QStringList{"clean-data"}, "Clean imported data."));
        parser.addOption(QCommandLineOption(QStringList{"vacuum-analyze"},
                                            "Run SQLite vacuum/analyze maintenance."));
        parser.addOption(QCommandLineOption(QStringList{"clean-orphan-derivations"},
                                            "Clean orphan derivation references."));
        parser.addOption(QCommandLineOption(
            QStringList{"acao"}, "Deprecated legacy action; use explicit commands.", "action"));
        parser.addOption(
            QCommandLineOption(QStringList{"log-level"}, "Set logging level.", "level"));
    }

    struct LogLevelRules final {
        int exitCode{0};
        std::string filterRules;
    };

    LogLevelRules parseLogLevel(const QString& level) {
        const auto normalized = level.toLower();
        if (normalized == "trace" || normalized == "debug") {
            return {
                0,
                "*.debug=true\n"
                "*.info=true\n"
                "*.warning=true\n"
                "*.critical=true\n",
            };
        }
        if (normalized == "info") {
            return {
                0,
                "*.debug=false\n"
                "*.info=true\n"
                "*.warning=true\n"
                "*.critical=true\n",
            };
        }
        if (normalized == "warning") {
            return {
                0,
                "*.debug=false\n"
                "*.info=false\n"
                "*.warning=true\n"
                "*.critical=true\n",
            };
        }
        if (normalized == "error" || normalized == "critical") {
            return {
                0,
                "*.debug=false\n"
                "*.info=false\n"
                "*.warning=false\n"
                "*.critical=true\n",
            };
        }
        if (normalized == "off") {
            return {
                0,
                "*.debug=false\n"
                "*.info=false\n"
                "*.warning=false\n"
                "*.critical=false\n",
            };
        }
        std::cerr << "error: invalid --log-level value, expected: "
                     "trace|debug|info|warning|error|critical|off\n";
        return {2, {}};
    }

    std::optional<std::filesystem::path> findDefaultDatabaseFromCurrentDirectory() {
        std::error_code error;
        auto directory = std::filesystem::current_path(error);
        if (error) {
            return std::nullopt;
        }

        while (!directory.empty()) {
            const auto candidate = directory / "data" / "ssas.db";
            if (std::filesystem::is_regular_file(candidate, error)) {
                return std::filesystem::weakly_canonical(candidate, error);
            }
            if (error) {
                error.clear();
            }
            const auto parent = directory.parent_path();
            if (parent == directory) {
                break;
            }
            directory = parent;
        }
        return std::nullopt;
    }

    void printMissingDatabaseMessage() {
        std::cerr << "error: missing required --db\n"
                  << "Usage:\n"
                  << "  ssa_consulta_rapida_cli --db <path-to-ssas.db> [options]\n"
                  << "Examples:\n"
                  << "  ssa_consulta_rapida_cli --db data/ssas.db\n"
                  << "  ssa_consulta_rapida_cli --db /absolute/path/ssas.db --search \"term\"\n";

        if (const auto defaultDatabase = findDefaultDatabaseFromCurrentDirectory()) {
            std::cerr << "Detected project database:\n"
                      << "  " << ssa::qt::toUtf8(*defaultDatabase) << '\n';
        } else {
            std::cerr << "No project database was found from the current directory.\n"
                      << "Place the database at <repo>/data/ssas.db or pass an explicit path.\n";
        }
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
            const auto logLevel = parseLogLevel(parser.value("log-level"));
            if (logLevel.exitCode != 0) {
                return logLevel.exitCode;
            }
            if (!logLevel.filterRules.empty()) {
                QLoggingCategory::setFilterRules(QString::fromStdString(logLevel.filterRules));
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
                const auto workflowValidation =
                    SsaCliWorkflowRunner::validateWorkflowRequest(parser);
                if (!workflowValidation.ok()) {
                    std::cerr << workflowValidation.message << '\n';
                    return 1;
                }
                if (SsaCliWorkflowRunner::requiresDatabase(parser)) {
                    if (!parser.isSet("db")) {
                        printMissingDatabaseMessage();
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
                printMissingDatabaseMessage();
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
                                     const application::SsaBrowseService& browse) {
        const auto record = browse.details(parser.value("details").toStdString());
        if (!record) {
            std::cerr << "ssa not found\n";
            return 1;
        }
        SsaCliPrinter{std::cout}.printDetails(*record);
        return 0;
    }

    int SsaCliController::runPage(const QCommandLineParser& parser,
                                  const application::SsaBrowseService& browse) {
        const auto requestedColumns = SsaCliRequestMapper::requestedColumns(parser);
        const auto outputColumns = browse.columnsOrDefault(requestedColumns);
        const auto request = SsaCliRequestMapper::pageRequest(parser, outputColumns);

        const auto page = browse.page(request);
        SsaCliPrinter{std::cout}.printPage(page, outputColumns);
        return 0;
    }

    int SsaCliController::runWorkflow(const QCommandLineParser& parser,
                                      const application::SsaWorkflowService& workflows) {
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
        request.outputPath = ssa::qt::toFileSystemPath(parser.value("export"));
        const auto result = workflows->exportFilteredList(request);
        std::cerr << result.message << '\n';
        return result.ok() ? 0 : 1;
    }

} // namespace ssa::app::cli
