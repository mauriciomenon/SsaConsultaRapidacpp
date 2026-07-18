#include "SsaCliController.h"
#include "ports/ISsaBrowsePort.h"
#include "ports/IWorkflowPorts.h"
#include "qt/FilesystemPath.h"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

    class CapturingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        [[nodiscard]] ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            std::stop_token = {}) override {
            (void)request;
            return {ssa::ports::WorkflowStatus::NotImplemented,
                    "import external files workflow adapter is unavailable"};
        }

        [[nodiscard]] ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest& request,
                                                        std::stop_token = {}) override {
            lastRequest = request;
            called = true;
            return {ssa::ports::WorkflowStatus::Succeeded, "rescan requested"};
        }

        ssa::ports::RescanRequest lastRequest;
        bool called = false;
    };

    class CapturingDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const override {
            return false;
        }

        [[nodiscard]] ssa::ports::WorkflowResult
        importDerivations(const ssa::ports::ImportDerivationsRequest&,
                          std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "derivadas import requested"};
        }

        [[nodiscard]] ssa::ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token = {}) override {
            called = true;
            return {ssa::ports::WorkflowStatus::Succeeded, "orphan derivation cleanup requested"};
        }

        bool called = false;
    };

    class UnusedBrowsePort final : public ssa::ports::ISsaBrowsePort {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest&,
                                        std::stop_token = {}) const override {
            return {};
        }
        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 0;
        }
        std::optional<ssa::domain::SsaRecord> details(const ssa::domain::SsaNumber&,
                                                      std::stop_token = {}) const override {
            return std::nullopt;
        }
        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }
        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }
        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }
        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }
    };

    class CapturingExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult
        exportFilteredList(const ssa::ports::ExportFilteredListRequest& request,
                           std::stop_token = {}) override {
            lastRequest = request;
            return {ssa::ports::WorkflowStatus::Succeeded, "exported"};
        }

        ssa::ports::ExportFilteredListRequest lastRequest;
    };

    std::shared_ptr<ssa::application::SsaBrowseService>
    unusedBrowseFactory(const std::filesystem::path& path) {
        (void)path;
        return {};
    }

    ssa::app::cli::SsaCliController
    controllerWithWorkflow(const std::shared_ptr<ssa::application::SsaWorkflowService>& workflows) {
        return ssa::app::cli::SsaCliController{
            unusedBrowseFactory, [workflows] { return workflows; },
            [workflows](const std::filesystem::path&, const std::filesystem::path&) {
                return workflows;
            }};
    }

    std::string existingDatabaseArgument() {
        const auto path = std::filesystem::temp_directory_path() / "ssa_cli_controller_test.db";
        {
            std::ofstream database(path);
        }
        return path.string();
    }

    class StderrCapture final {
      public:
        StderrCapture() : previous_(std::cerr.rdbuf(buffer_.rdbuf())) {}

        ~StderrCapture() {
            std::cerr.rdbuf(previous_);
        }

        [[nodiscard]] std::string text() const {
            return buffer_.str();
        }

      private:
        std::ostringstream buffer_;
        std::streambuf* previous_;
    };

} // namespace

TEST_CASE("cli prints actionable database guidance when db is missing") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);
    StderrCapture stderrCapture;

    const int exitCode = controller.run({"ssa"});

    REQUIRE(exitCode == 2);
    const auto output = stderrCapture.text();
    REQUIRE(output.find("error: missing required --db") != std::string::npos);
    REQUIRE(output.find("ssa_consulta_rapida_cli --db <path-to-ssas.db>") != std::string::npos);
    REQUIRE((output.find("Detected project database:") != std::string::npos ||
             output.find("Place the database at <repo>/data/ssas.db") != std::string::npos));
}

TEST_CASE("cli reports invalid database path with the provided path") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto missingPath =
        (std::filesystem::temp_directory_path() / "ssa_cli_missing_database.db").string();
    StderrCapture stderrCapture;

    const int exitCode = controller.run({"ssa", "--db", missingPath.c_str()});

    REQUIRE(exitCode == 1);
    const auto output = stderrCapture.text();
    REQUIRE(output.find("database path does not exist: " + missingPath) != std::string::npos);
}

TEST_CASE("cli treats rescan as full rescan compatibility alias") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode =
        controller.run({"ssa", "--rescan", "--db", databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Full);
}

TEST_CASE("cli exposes explicit incremental rescan") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run(
        {"ssa", "--incremental-rescan", "--db", databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Incremental);
}

TEST_CASE("cli propagates bounded import execution parameters") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();

    const int exitCode = controller.run({"ssa", "--incremental-rescan", "--import-chunk-rows",
                                         "128", "--sqlite-busy-wait-ms", "250", "--db",
                                         databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    CHECK(importPort->lastRequest.execution.rowsPerChunk == 128);
    CHECK(importPort->lastRequest.execution.sqliteBusyWait == std::chrono::milliseconds{250});
}

TEST_CASE("cli rejects invalid import execution parameters before the workflow") {
    SECTION("nonnumeric chunk size") {
        auto importPort = std::make_shared<CapturingImportPort>();
        auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
        const auto controller = controllerWithWorkflow(workflows);
        const auto databasePath = existingDatabaseArgument();
        StderrCapture stderrCapture;

        const int exitCode =
            controller.run({"ssa", "--incremental-rescan", "--import-chunk-rows", "many", "--db",
                            databasePath.c_str(), "--docs-dir", "."});

        CHECK(exitCode == 1);
        CHECK_FALSE(importPort->called);
        CHECK(stderrCapture.text().find("invalid import execution options") != std::string::npos);
    }

    SECTION("busy wait outside retry granularity") {
        auto importPort = std::make_shared<CapturingImportPort>();
        auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
        const auto controller = controllerWithWorkflow(workflows);
        const auto databasePath = existingDatabaseArgument();
        StderrCapture stderrCapture;

        const int exitCode = controller.run({"ssa", "--incremental-rescan", "--sqlite-busy-wait-ms",
                                             "7", "--db", databasePath.c_str(), "--docs-dir", "."});

        CHECK(exitCode == 1);
        CHECK_FALSE(importPort->called);
        CHECK(stderrCapture.text().find("invalid import execution options") != std::string::npos);
    }

    SECTION("import execution options without a rescan command") {
        auto importPort = std::make_shared<CapturingImportPort>();
        auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
        const auto controller = controllerWithWorkflow(workflows);
        const auto databasePath = existingDatabaseArgument();
        StderrCapture stderrCapture;

        const int exitCode = controller.run(
            {"ssa", "--import-chunk-rows", "10", "--db", databasePath.c_str(), "--docs-dir", "."});

        CHECK(exitCode == 1);
        CHECK_FALSE(importPort->called);
        CHECK(stderrCapture.text().find("require a rescan command") != std::string::npos);
    }
}

TEST_CASE("cli preserves unicode database and import paths") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QString unicodeRoot = directory.filePath(QString::fromUtf8("cli-unicode-\xE6\xBC\xA2"));
    const QString databasePath = QDir(unicodeRoot).filePath(QStringLiteral("ssas.db"));
    const QString docsPath =
        QDir(unicodeRoot).filePath(QString::fromUtf8("documentos-\xE6\xBC\xA2"));
    REQUIRE(QDir{}.mkpath(docsPath));
    QFile database(databasePath);
    REQUIRE(database.open(QIODevice::WriteOnly));
    database.close();

    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
    std::filesystem::path capturedDatabasePath;
    std::filesystem::path capturedDocsPath;
    const ssa::app::cli::SsaCliController controller{
        unusedBrowseFactory, [workflows] { return workflows; },
        [&](const std::filesystem::path& databaseValue, const std::filesystem::path& docsValue) {
            capturedDatabasePath = databaseValue;
            capturedDocsPath = docsValue;
            return workflows;
        }};

    const int exitCode = controller.run(
        {"ssa", "--incremental-rescan", "--db", databasePath, "--docs-dir", docsPath});

    REQUIRE(exitCode == 0);
    REQUIRE(capturedDatabasePath == ssa::qt::toFileSystemPath(databasePath));
    REQUIRE(capturedDocsPath == ssa::qt::toFileSystemPath(docsPath));
}

TEST_CASE("cli accepts --log-level values") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const int exitCode = controller.run({"ssa", "--log-level", "info", "--version"});
    REQUIRE(exitCode == 0);
}

TEST_CASE("cli rejects invalid --log-level values") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const int exitCode = controller.run({"ssa", "--log-level", "invalid", "--version"});
    REQUIRE(exitCode == 2);
}

TEST_CASE("cli accepts --cols as alias for --columns") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);
    const int exitCode = controller.run({"ssa", "--cols", "numero_ssa", "--version"});

    REQUIRE(exitCode == 0);
}

TEST_CASE("cli passes display labels explicitly to csv export") {
    const auto browse =
        std::make_shared<ssa::application::SsaBrowseService>(std::make_shared<UnusedBrowsePort>());
    const auto exportPort = std::make_shared<CapturingExportPort>();
    const auto workflows =
        std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
    const ssa::app::cli::SsaCliController controller{
        [browse](const std::filesystem::path&) { return browse; },
        [workflows] { return workflows; },
        [workflows](const std::filesystem::path&, const std::filesystem::path&) {
            return workflows;
        }};
    const auto databasePath = existingDatabaseArgument();
    const auto outputPath =
        (std::filesystem::temp_directory_path() / "ssa_cli_export_labels.csv").string();

    const int exitCode = controller.run({"ssa", "--db", databasePath.c_str(), "--export",
                                         outputPath.c_str(), "--columns", "numero_ssa,situacao"});

    REQUIRE(exitCode == 0);
    REQUIRE(exportPort->lastRequest.query.visibleColumns ==
            std::vector<std::string>{"numero_ssa", "situacao"});
    REQUIRE(exportPort->lastRequest.headerLabels == std::vector<std::string>{"No SSA", "Sit."});
}

TEST_CASE("cli rejects legacy acao backfill with orphan cleanup guidance") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    StderrCapture stderrCapture;
    const int exitCode =
        controller.run({"ssa", "--acao", "backfill", "--db", databasePath.c_str()});

    REQUIRE(exitCode == 1);
    REQUIRE_FALSE(derivadasPort->called);
    REQUIRE(stderrCapture.text().find("--clean-orphan-derivations") != std::string::npos);
}

TEST_CASE("cli executes orphan derivation cleanup command") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode =
        controller.run({"ssa", "--clean-orphan-derivations", "--db", databasePath.c_str()});

    REQUIRE(exitCode == 0);
    REQUIRE(derivadasPort->called);
}

TEST_CASE("cli rejects unsupported acao commands") {
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr,
        std::make_shared<CapturingDerivadasPort>());

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run({"ssa", "--acao", "invalid", "--db", databasePath.c_str()});

    REQUIRE(exitCode == 1);
}

TEST_CASE("cli rejects unsupported acao before requiring db") {
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr,
        std::make_shared<CapturingDerivadasPort>());

    const auto controller = controllerWithWorkflow(workflows);
    const int exitCode = controller.run({"ssa", "--acao", "invalid"});

    REQUIRE(exitCode == 1);
}

TEST_CASE("cli rejects acao with another workflow command") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run(
        {"ssa", "--clean-orphan-derivations", "--acao", "backfill", "--db", databasePath.c_str()});

    REQUIRE(exitCode == 1);
    REQUIRE_FALSE(derivadasPort->called);
}

TEST_CASE("cli rejects multiple workflow commands") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run(
        {"ssa", "--rescan", "--reset-db", "--db", databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 1);
    REQUIRE_FALSE(importPort->called);
}
