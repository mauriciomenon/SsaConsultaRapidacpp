#include "SsaCliWorkflowRunner.h"

#include <array>
#include <string>

namespace {

    struct WorkflowCliCommand {
        const char* option;
        ssa::ports::WorkflowResult (*run)(const QCommandLineParser&,
                                          const ssa::application::SsaWorkflowService&);
    };

    ssa::ports::WorkflowResult rejectMultipleWorkflowCommands() {
        return {ssa::ports::WorkflowStatus::Rejected,
                "select only one workflow command per execution"};
    }

    ssa::ports::WorkflowResult rejectUnsupportedAction(const QString& action) {
        return {ssa::ports::WorkflowStatus::Rejected,
                "unsupported --acao value: " + action.toStdString()};
    }

    bool isBackfillAction(const QString& action) {
        return action.toLower() == "backfill";
    }

    bool usesDefaultOptimizedImport(const QCommandLineParser& parser) {
        if (parser.isSet("optimized")) {
            return true;
        }
        if (parser.isSet("standard")) {
            return false;
        }
        return true;
    }

    ssa::ports::WorkflowResult
    incrementalRescan(const QCommandLineParser& parser,
                      const ssa::application::SsaWorkflowService& workflows) {
        return workflows.rescan(
            {ssa::ports::RescanMode::Incremental, true, usesDefaultOptimizedImport(parser)});
    }

    ssa::ports::WorkflowResult fullRescan(const QCommandLineParser& parser,
                                          const ssa::application::SsaWorkflowService& workflows) {
        return workflows.rescan(
            {ssa::ports::RescanMode::Full, true, usesDefaultOptimizedImport(parser)});
    }

    // Python compatibility: --rescan is an alias for --force-rescan.
    constexpr std::array<WorkflowCliCommand, 7> kWorkflowCommands{{
        {"rescan", fullRescan},
        {"force-rescan", fullRescan},
        {"incremental-rescan", incrementalRescan},
        {"reset-db",
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.resetDatabase();
         }},
        {"clean-data",
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.cleanData();
         }},
        {"vacuum-analyze",
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.vacuumAnalyze();
         }},
        {"sync-derivadas",
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.syncDerivadas();
         }},
    }};

    constexpr std::array<const char*, 7> kDatabaseWorkflowCommands{{
        "rescan",
        "force-rescan",
        "incremental-rescan",
        "reset-db",
        "clean-data",
        "vacuum-analyze",
        "sync-derivadas",
    }};

    bool isAcaoRequested(const QCommandLineParser& parser) {
        return parser.isSet("acao");
    }

    bool isBackfillActionRequested(const QCommandLineParser& parser) {
        return isAcaoRequested(parser) && isBackfillAction(parser.value("acao"));
    }

} // namespace

namespace ssa::app::cli {

    bool SsaCliWorkflowRunner::hasWorkflowCommand(const QCommandLineParser& parser) {
        for (const auto& command : kWorkflowCommands) {
            if (parser.isSet(command.option)) {
                return true;
            }
        }
        if (isAcaoRequested(parser)) {
            return true;
        }
        return false;
    }

    bool SsaCliWorkflowRunner::requiresDatabase(const QCommandLineParser& parser) {
        for (const auto* option : kDatabaseWorkflowCommands) {
            if (parser.isSet(option)) {
                return true;
            }
        }
        if (isAcaoRequested(parser)) {
            return true;
        }
        return false;
    }

    ports::WorkflowResult
    SsaCliWorkflowRunner::runSelected(const QCommandLineParser& parser,
                                      const application::SsaWorkflowService& workflows) {
        int selectedCommands = 0;
        const WorkflowCliCommand* selectedCommand = nullptr;
        const bool hasAcao = isAcaoRequested(parser);
        if (hasAcao && !isBackfillActionRequested(parser)) {
            return rejectUnsupportedAction(parser.value("acao"));
        }
        if (hasAcao) {
            ++selectedCommands;
        }
        for (const auto& command : kWorkflowCommands) {
            if (parser.isSet(command.option)) {
                ++selectedCommands;
                selectedCommand = &command;
            }
        }
        if (selectedCommands > 1) {
            return rejectMultipleWorkflowCommands();
        }
        if (selectedCommand != nullptr) {
            return selectedCommand->run(parser, workflows);
        }
        if (hasAcao) {
            return workflows.syncDerivadas();
        }
        return {ports::WorkflowStatus::Rejected, "no workflow command selected"};
    }

} // namespace ssa::app::cli
