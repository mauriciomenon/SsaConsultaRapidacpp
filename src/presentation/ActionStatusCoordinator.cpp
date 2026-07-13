#include "presentation/ActionStatusCoordinator.h"

namespace ssa::presentation {

    namespace {

        const QString kCommandRunningMessage{"Executando comando..."};
        const QString kCommandSuccessMessage{"Comando concluido"};
        const QString kCommandFailedMessage{"Falha ao executar comando"};
        const QString kExportRunningMessage{"Exportando dados..."};
        const QString kExportSuccessMessage{"Exportacao concluida"};
        const QString kExportFailedMessage{"Falha ao exportar dados"};
        const QString kPreferencesFailedMessage{"Falha ao salvar preferencias"};

        struct ActionStatusMessages {
            QString success;
            QString failure;
        };

        void clearAndSetError(StatusViewModel& status, const QString& message) {
            status.setError({});
            status.setMessage(message);
        }

        void reportRunning(StatusViewModel& status, const bool running, const QString& message) {
            if (running) {
                clearAndSetError(status, message);
            }
        }

        void reportResult(StatusViewModel& status, const bool succeeded, const QString& detail,
                          const ActionStatusMessages& messages, const bool warning = false) {
            if (succeeded) {
                status.setError(warning ? detail : QString{});
                status.setMessage(messages.success);
            } else {
                status.setError(detail);
                status.setMessage(messages.failure);
            }
        }

    } // namespace

    ActionStatusCoordinator::ActionStatusCoordinator(
        CommandViewModel& commands, ExportViewModel& exports, WorkflowCommandViewModel& workflows,
        StatusViewModel& status, UserPreferencesCoordinator& preferences, QObject* parent)
        : QObject(parent), commands_(commands), exports_(exports), workflows_(workflows),
          status_(status), preferences_(preferences) {
        connect(&commands_, &CommandViewModel::runningChanged, this,
                &ActionStatusCoordinator::onCommandRunning);
        connect(&commands_, &CommandViewModel::lastResultChanged, this,
                &ActionStatusCoordinator::onCommandResult);
        connect(&exports_, &ExportViewModel::runningChanged, this,
                &ActionStatusCoordinator::onExportRunning);
        connect(&exports_, &ExportViewModel::lastResultChanged, this,
                &ActionStatusCoordinator::onExportResult);
        connect(&workflows_, &WorkflowCommandViewModel::stateChanged, this,
                &ActionStatusCoordinator::onWorkflowRunning);
        connect(&workflows_, &WorkflowCommandViewModel::lastResultChanged, this,
                &ActionStatusCoordinator::onWorkflowResult);
        connect(&preferences_, &UserPreferencesCoordinator::saveFailed, this,
                &ActionStatusCoordinator::onPreferenceSaveFailed);
    }

    void ActionStatusCoordinator::onCommandRunning() {
        reportRunning(status_, commands_.running(), kCommandRunningMessage);
    }

    void ActionStatusCoordinator::onCommandResult() {
        reportResult(status_, commands_.lastSucceeded(), commands_.lastMessage(),
                     {kCommandSuccessMessage, kCommandFailedMessage});
    }

    void ActionStatusCoordinator::onExportRunning() {
        reportRunning(status_, exports_.running(), kExportRunningMessage);
    }

    void ActionStatusCoordinator::onExportResult() {
        if (exports_.lastStatus() == QStringLiteral("canceled")) {
            clearAndSetError(status_, exports_.lastMessage());
            return;
        }
        reportResult(status_, exports_.lastSucceeded(), exports_.lastMessage(),
                     {kExportSuccessMessage, kExportFailedMessage});
    }

    void ActionStatusCoordinator::onWorkflowRunning() {
        reportRunning(status_, workflows_.running(), workflows_.runningMessage());
    }

    void ActionStatusCoordinator::onWorkflowResult() {
        if (workflows_.lastCanceled()) {
            clearAndSetError(status_, workflows_.lastMessage());
            return;
        }
        reportResult(status_, workflows_.lastSucceeded(), workflows_.lastMessage(),
                     {workflows_.successMessage(), workflows_.failureMessage()},
                     workflows_.lastWarning());
    }

    void ActionStatusCoordinator::onPreferenceSaveFailed(const QString& message) {
        status_.setError(message);
        status_.setMessage(kPreferencesFailedMessage);
    }

} // namespace ssa::presentation
