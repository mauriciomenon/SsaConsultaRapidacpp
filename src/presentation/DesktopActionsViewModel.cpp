#include "presentation/DesktopActionsViewModel.h"

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    namespace {

        std::shared_ptr<ports::IExternalCommandPort>
        requireCommandPort(std::shared_ptr<ports::IExternalCommandPort> commandPort) {
            if (!commandPort) {
                throw std::invalid_argument("external command port is required");
            }
            return commandPort;
        }

    } // namespace

    DesktopActionsViewModel::DesktopActionsViewModel(
        std::shared_ptr<ports::IExternalCommandPort> commandPort,
        std::shared_ptr<application::SsaWorkflowService> workflowService,
        ExportViewModel::RequestFactory requestFactory, StatusViewModel& status,
        UserPreferencesCoordinator& preferences, QObject* parent)
        : QObject(parent), commands_(requireCommandPort(std::move(commandPort)), this),
          exports_(workflowService, std::move(requestFactory), nullptr, this),
          workflows_(std::move(workflowService), this), currentWeek_(this),
          statusCoordinator_(commands_, exports_, workflows_, status, preferences, this) {}

    CommandViewModel* DesktopActionsViewModel::commands() {
        return &commands_;
    }

    ExportViewModel* DesktopActionsViewModel::exports() {
        return &exports_;
    }

    WorkflowCommandViewModel* DesktopActionsViewModel::workflows() {
        return &workflows_;
    }

    CurrentWeekViewModel* DesktopActionsViewModel::currentWeek() {
        return &currentWeek_;
    }

} // namespace ssa::presentation
