#pragma once

#include "application/SsaBrowseService.h"
#include "application/SsaWorkflowService.h"

#include <QCommandLineParser>
#include <QStringList>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace ssa::app::cli {

    class SsaCliController final {
      public:
        using BrowseFactory = std::function<std::shared_ptr<application::SsaBrowseService>(
            const std::filesystem::path&)>;
        using WorkflowFactory = std::function<std::shared_ptr<application::SsaWorkflowService>()>;
        using DatabaseWorkflowFactory =
            std::function<std::shared_ptr<application::SsaWorkflowService>(
                const std::filesystem::path&, const std::filesystem::path&)>;

        SsaCliController(BrowseFactory browseFactory, WorkflowFactory workflowFactory,
                         DatabaseWorkflowFactory databaseWorkflowFactory);

        int run(const QStringList& arguments) const;

      private:
        [[nodiscard]] std::shared_ptr<application::SsaBrowseService>
        createBrowseService(const QCommandLineParser& parser) const;
        [[nodiscard]] int runDetails(const QCommandLineParser& parser,
                                     const application::SsaBrowseService& browse) const;
        [[nodiscard]] int runPage(const QCommandLineParser& parser,
                                  const application::SsaBrowseService& browse) const;
        [[nodiscard]] int runWorkflow(const QCommandLineParser& parser,
                                      const application::SsaWorkflowService& workflows) const;
        [[nodiscard]] int runExport(const QCommandLineParser& parser,
                                    const std::filesystem::path& databasePath) const;
        BrowseFactory browseFactory_;
        WorkflowFactory workflowFactory_;
        DatabaseWorkflowFactory databaseWorkflowFactory_;
    };

} // namespace ssa::app::cli
