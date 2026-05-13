#pragma once

#include "platform/LocalOpenCommandHandler.h"
#include "platform/SamCommandHandler.h"
#include "ports/IExternalCommandPort.h"

#include <QUrl>

#include <filesystem>
#include <vector>

namespace ssa::platform {

    class DesktopExternalCommandPort final : public ports::IExternalCommandPort {
      public:
        explicit DesktopExternalCommandPort(
            QUrl samBaseUrl, LocalOpenPaths paths = {},
            std::vector<std::filesystem::path> allowedOpenRoots = {});

        [[nodiscard]] ports::ExternalCommandResult
        execute(const ports::ExternalCommand& command) override;

      private:
        [[nodiscard]] ports::ExternalCommandResult
        handleOpenSamHome(const ports::ExternalCommand& command);
        [[nodiscard]] ports::ExternalCommandResult
        handleOpenSsa(const ports::ExternalCommand& command);
        [[nodiscard]] ports::ExternalCommandResult
        handleOpenPath(const ports::ExternalCommand& command);
        SamCommandHandler sam_;
        LocalOpenCommandHandler localOpen_;
    };

} // namespace ssa::platform
