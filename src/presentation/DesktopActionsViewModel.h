#pragma once

#include "application/SsaWorkflowService.h"
#include "ports/IExternalCommandPort.h"
#include "presentation/ActionStatusCoordinator.h"
#include "presentation/CommandViewModel.h"
#include "presentation/CurrentWeekViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/StatusViewModel.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "presentation/WorkflowCommandViewModel.h"

#include <QObject>

#include <memory>

namespace ssa::presentation {

    class DesktopActionsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(CommandViewModel* commands READ commands CONSTANT)
        Q_PROPERTY(ExportViewModel* exports READ exports CONSTANT)
        Q_PROPERTY(WorkflowCommandViewModel* workflows READ workflows CONSTANT)
        Q_PROPERTY(CurrentWeekViewModel* currentWeek READ currentWeek CONSTANT)

      public:
        DesktopActionsViewModel(std::shared_ptr<ports::IExternalCommandPort> commandPort,
                                std::shared_ptr<application::SsaWorkflowService> workflowService,
                                ExportViewModel::RequestFactory requestFactory,
                                StatusViewModel& status, UserPreferencesCoordinator& preferences,
                                QObject* parent = nullptr);

        [[nodiscard]] CommandViewModel* commands();
        [[nodiscard]] ExportViewModel* exports();
        [[nodiscard]] WorkflowCommandViewModel* workflows();
        [[nodiscard]] CurrentWeekViewModel* currentWeek();

      private:
        CommandViewModel commands_;
        ExportViewModel exports_;
        WorkflowCommandViewModel workflows_;
        CurrentWeekViewModel currentWeek_;
        ActionStatusCoordinator statusCoordinator_;
    };

} // namespace ssa::presentation
