#pragma once

#include "presentation/CommandViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/StatusViewModel.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "presentation/WorkflowCommandViewModel.h"

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class ActionStatusCoordinator final : public QObject {
        Q_OBJECT

      public:
        ActionStatusCoordinator(CommandViewModel& commands, ExportViewModel& exports,
                                WorkflowCommandViewModel& workflows, StatusViewModel& status,
                                UserPreferencesCoordinator& preferences, QObject* parent = nullptr);

      private:
        void onCommandRunning();
        void onCommandResult();
        void onExportRunning();
        void onExportResult();
        void onWorkflowRunning();
        void onWorkflowResult();
        void onPreferenceSaveFailed(const QString& message);

        CommandViewModel& commands_;
        ExportViewModel& exports_;
        WorkflowCommandViewModel& workflows_;
        StatusViewModel& status_;
        UserPreferencesCoordinator& preferences_;
    };

} // namespace ssa::presentation
