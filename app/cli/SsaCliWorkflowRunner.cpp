#include "SsaCliWorkflowRunner.h"

#include <array>
#include <string>

namespace {

    ssa::ports::WorkflowResult runBackfill(const QCommandLineParser&,
                                           const ssa::application::SsaWorkflowService& workflows) {
        return workflows.syncDerivadas();
    }

    struct WorkflowCliCommand {
        const char* option;
        bool requiresDatabase;
        ssa::ports::WorkflowResult (*run)(const QCommandLineParser&,
                                          const ssa::application::SsaWorkflowService&);
    };

    struct WorkflowCommandSelection final {
        int selectedCommands{0};
        const WorkflowCliCommand* selectedCommand{nullptr};
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

    bool shouldOptimizeImport(const QCommandLineParser& parser) {
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
            {ssa::ports::RescanMode::Incremental, true, shouldOptimizeImport(parser)});
    }

    ssa::ports::WorkflowResult fullRescan(const QCommandLineParser& parser,
                                          const ssa::application::SsaWorkflowService& workflows) {
        return workflows.rescan({ssa::ports::RescanMode::Full, true, shouldOptimizeImport(parser)});
    }

    // Python compatibility: --rescan is an alias for --force-rescan.
    constexpr std::array<WorkflowCliCommand, 7> kWorkflowCommands{{
        {"rescan", true, fullRescan},
        {"force-rescan", true, fullRescan},
        {"incremental-rescan", true, incrementalRescan},
        {"reset-db", true,
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.resetDatabase();
         }},
        {"clean-data", true,
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.cleanData();
         }},
        {"vacuum-analyze", true,
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.vacuumAnalyze();
         }},
        {"sync-derivadas", true,
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.syncDerivadas();
         }},
    }};

    constexpr WorkflowCliCommand kAcaoBackfillCommand{"acao-backfill", true, runBackfill};

    bool isAcaoRequested(const QCommandLineParser& parser) {
        return parser.isSet("acao");
    }

    bool isBackfillActionRequested(const QCommandLineParser& parser) {
        return isAcaoRequested(parser) && isBackfillAction(parser.value("acao"));
    }

    WorkflowCommandSelection selectedWorkflowCommand(const QCommandLineParser& parser) {
        WorkflowCommandSelection selection;
        if (isBackfillActionRequested(parser)) {
            ++selection.selectedCommands;
            selection.selectedCommand = &kAcaoBackfillCommand;
        }
        for (const auto& command : kWorkflowCommands) {
            if (parser.isSet(command.option)) {
                ++selection.selectedCommands;
                selection.selectedCommand = &command;
            }
        }
        return selection;
    }

    ssa::ports::WorkflowResult
    validateWorkflowSelection(const QCommandLineParser& parser,
                              const WorkflowCommandSelection& selection) {
        const bool hasAcao = isAcaoRequested(parser);
        if (hasAcao && !isBackfillActionRequested(parser)) {
            return rejectUnsupportedAction(parser.value("acao"));
        }
        if (selection.selectedCommands > 1) {
            return rejectMultipleWorkflowCommands();
        }
        if (!selection.selectedCommand) {
            return {ssa::ports::WorkflowStatus::Rejected, "no workflow command selected"};
        }
        return {ssa::ports::WorkflowStatus::Succeeded, "workflow command validated"};
    }

} // namespace

namespace ssa::app::cli {

    bool SsaCliWorkflowRunner::hasWorkflowCommand(const QCommandLineParser& parser) {
        if (isAcaoRequested(parser)) {
            return true;
        }
        for (const auto& command : kWorkflowCommands) {
            if (parser.isSet(command.option)) {
                return true;
            }
        }
        return false;
    }

    bool SsaCliWorkflowRunner::requiresDatabase(const QCommandLineParser& parser) {
        if (isAcaoRequested(parser)) {
            return kAcaoBackfillCommand.requiresDatabase;
        }
        for (const auto& command : kWorkflowCommands) {
            if (parser.isSet(command.option)) {
                return command.requiresDatabase;
            }
        }
        return false;
    }

    ports::WorkflowResult
    SsaCliWorkflowRunner::validateWorkflowRequest(const QCommandLineParser& parser) {
        return validateWorkflowSelection(parser, selectedWorkflowCommand(parser));
    }

    ports::WorkflowResult
    SsaCliWorkflowRunner::runSelected(const QCommandLineParser& parser,
                                      const application::SsaWorkflowService& workflows) {
        const auto selection = selectedWorkflowCommand(parser);
        const auto requestValidation = validateWorkflowSelection(parser, selection);
        if (!requestValidation.ok()) {
            return requestValidation;
        }
        const WorkflowCliCommand* selectedCommand = selection.selectedCommand;
        if (selectedCommand != nullptr) {
            return selectedCommand->run(parser, workflows);
        }
        return {ports::WorkflowStatus::Rejected, "no workflow command selected"};
    }

} // namespace ssa::app::cli
