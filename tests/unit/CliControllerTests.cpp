#include "SsaCliController.h"
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
        [[nodiscard]] ssa::ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token = {}) override {
            called = true;
            return {ssa::ports::WorkflowStatus::Succeeded, "orphan derivation cleanup requested"};
        }

        bool called = false;
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
