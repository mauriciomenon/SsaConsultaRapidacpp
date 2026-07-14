#include "SsaCliWorkflowRunner.h"

#include <algorithm>
#include <array>
#include <string>

namespace {

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
        if (action.compare(QStringLiteral("backfill"), Qt::CaseInsensitive) == 0) {
            return {ssa::ports::WorkflowStatus::Rejected,
                    "--acao backfill is deprecated; use --clean-orphan-derivations"};
        }
        return {ssa::ports::WorkflowStatus::Rejected,
                "unsupported --acao value: " + action.toStdString()};
    }

    ssa::ports::WorkflowResult
    incrementalRescan(const QCommandLineParser&,
                      const ssa::application::SsaWorkflowService& workflows) {
        return workflows.rescan({ssa::ports::RescanMode::Incremental});
    }

    ssa::ports::WorkflowResult fullRescan(const QCommandLineParser&,
                                          const ssa::application::SsaWorkflowService& workflows) {
        return workflows.rescan({ssa::ports::RescanMode::Full});
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
        {"clean-orphan-derivations", true,
         [](const QCommandLineParser&, const ssa::application::SsaWorkflowService& workflows) {
             return workflows.cleanOrphanDerivations();
         }},
    }};

    bool isAcaoRequested(const QCommandLineParser& parser) {
        return parser.isSet("acao");
    }

    WorkflowCommandSelection selectedWorkflowCommand(const QCommandLineParser& parser) {
        WorkflowCommandSelection selection;
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
        if (hasAcao) {
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
        return isAcaoRequested(parser) ||
               std::ranges::any_of(kWorkflowCommands, [&parser](const auto& command) {
                   return parser.isSet(command.option);
               });
    }

    bool SsaCliWorkflowRunner::requiresDatabase(const QCommandLineParser& parser) {
        const auto command = std::ranges::find_if(
            kWorkflowCommands, [&parser](const auto& item) { return parser.isSet(item.option); });
        if (command != kWorkflowCommands.end()) {
            return command->requiresDatabase;
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
        auto requestValidation = validateWorkflowSelection(parser, selection);
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
