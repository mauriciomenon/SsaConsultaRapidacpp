#pragma once

#include <map>
#include <string>
#include <vector>

namespace ssa::ports {

    enum class ExternalCommandKind {
        OpenSamHome,
        OpenSsa,
        OpenPath,
        OpenInputFolder,
        OpenProcessedFolder,
        OpenRedundantFolder,
        OpenInstallationGuide,
    };

    enum class ExternalCommandStatus {
        Succeeded,
        NotImplemented,
        Rejected,
        Failed,
    };

    struct ExternalCommand {
        ExternalCommandKind kind{ExternalCommandKind::OpenSamHome};
        std::map<std::string, std::string> parameters;
    };

    struct ExternalCommandResult {
        ExternalCommandStatus status{ExternalCommandStatus::Succeeded};
        std::string message;

        [[nodiscard]] bool ok() const noexcept {
            return status == ExternalCommandStatus::Succeeded;
        }
    };

    class IExternalCommandPort {
      public:
        virtual ~IExternalCommandPort() = default;

        [[nodiscard]] virtual ExternalCommandResult execute(const ExternalCommand& command) = 0;
    };

} // namespace ssa::ports
