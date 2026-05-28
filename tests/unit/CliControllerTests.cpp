#include "SsaCliController.h"
#include "ports/IWorkflowPorts.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

namespace {

    class CapturingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        [[nodiscard]] ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request) override {
            (void)request;
            return {ssa::ports::WorkflowStatus::NotImplemented,
                    "import external files workflow adapter is unavailable"};
        }

        [[nodiscard]] ssa::ports::WorkflowResult
        rescan(const ssa::ports::RescanRequest& request) override {
            lastRequest = request;
            called = true;
            return {ssa::ports::WorkflowStatus::Succeeded, "rescan requested"};
        }

        ssa::ports::RescanRequest lastRequest;
        bool called{false};
    };

    class CapturingDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        [[nodiscard]] ssa::ports::WorkflowResult syncDerivadas() override {
            called = true;
            return {ssa::ports::WorkflowStatus::Succeeded, "sync derivadas requested"};
        }

        bool called{false};
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
            std::ofstream database{path};
        }
        return path.string();
    }

} // namespace

TEST_CASE("cli treats rescan as full rescan compatibility alias with optimized default") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode =
        controller.run({"ssa", "--rescan", "--db", databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Full);
    REQUIRE(importPort->lastRequest.optimized);
}

TEST_CASE("cli exposes explicit incremental rescan") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run({"ssa", "--incremental-rescan", "--standard", "--db",
                                         databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Incremental);
    REQUIRE_FALSE(importPort->lastRequest.optimized);
}

TEST_CASE("cli lets optimized override standard import strategy") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const auto controller = controllerWithWorkflow(workflows);

    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run({"ssa", "--rescan", "--optimized", "--standard", "--db",
                                         databasePath.c_str(), "--docs-dir", "."});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.optimized);
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

TEST_CASE("cli maps acao backfill to sync derivadas command") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode =
        controller.run({"ssa", "--acao", "backfill", "--db", databasePath.c_str()});

    REQUIRE(exitCode == 0);
    REQUIRE(derivadasPort->called);
}

TEST_CASE("cli executes sync-derivadas command") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run({"ssa", "--sync-derivadas", "--db", databasePath.c_str()});

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

TEST_CASE("cli rejects acao with another workflow command") {
    auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
        std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);

    const auto controller = controllerWithWorkflow(workflows);
    const auto databasePath = existingDatabaseArgument();
    const int exitCode = controller.run(
        {"ssa", "--sync-derivadas", "--acao", "backfill", "--db", databasePath.c_str()});

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
