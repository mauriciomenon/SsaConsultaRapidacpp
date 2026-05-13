#include "SsaCliController.h"
#include "ports/IWorkflowPorts.h"

#include <catch2/catch_test_macros.hpp>

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

    std::shared_ptr<ssa::application::SsaBrowseService>
    unusedBrowseFactory(const std::filesystem::path& path) {
        (void)path;
        return {};
    }

} // namespace

TEST_CASE("cli treats rescan as full rescan compatibility alias") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const ssa::app::cli::SsaCliController controller(
        unusedBrowseFactory, [workflows] { return workflows; },
        [workflows](const std::filesystem::path&) { return workflows; });

    const int exitCode = controller.run({"ssa", "--rescan"});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Full);
    REQUIRE(importPort->lastRequest.optimized);
}

TEST_CASE("cli exposes explicit incremental rescan") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const ssa::app::cli::SsaCliController controller(
        unusedBrowseFactory, [workflows] { return workflows; },
        [workflows](const std::filesystem::path&) { return workflows; });

    const int exitCode = controller.run({"ssa", "--incremental-rescan", "--standard"});

    REQUIRE(exitCode == 0);
    REQUIRE(importPort->called);
    REQUIRE(importPort->lastRequest.mode == ssa::ports::RescanMode::Incremental);
    REQUIRE_FALSE(importPort->lastRequest.optimized);
}

TEST_CASE("cli rejects conflicting import strategy flags") {
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>();

    const ssa::app::cli::SsaCliController controller(
        unusedBrowseFactory, [workflows] { return workflows; },
        [workflows](const std::filesystem::path&) { return workflows; });

    const int exitCode = controller.run({"ssa", "--rescan", "--optimized", "--standard"});

    REQUIRE(exitCode == 1);
}

TEST_CASE("cli rejects multiple workflow commands") {
    auto importPort = std::make_shared<CapturingImportPort>();
    auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);

    const ssa::app::cli::SsaCliController controller(
        unusedBrowseFactory, [workflows] { return workflows; },
        [workflows](const std::filesystem::path&) { return workflows; });

    const int exitCode = controller.run({"ssa", "--rescan", "--reset-db"});

    REQUIRE(exitCode == 1);
    REQUIRE_FALSE(importPort->called);
}
