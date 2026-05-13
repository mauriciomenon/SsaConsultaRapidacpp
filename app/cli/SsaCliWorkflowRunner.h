#pragma once

#include "application/SsaWorkflowService.h"

#include <QCommandLineParser>

namespace ssa::app::cli {

    class SsaCliWorkflowRunner final {
      public:
        [[nodiscard]] static bool hasWorkflowCommand(const QCommandLineParser& parser);
        [[nodiscard]] static bool requiresDatabase(const QCommandLineParser& parser);
        [[nodiscard]] static ports::WorkflowResult
        runSelected(const QCommandLineParser& parser,
                    const application::SsaWorkflowService& workflows);
    };

} // namespace ssa::app::cli
