#pragma once

#include "platform/OpenPathPolicy.h"
#include "ports/IExternalCommandPort.h"

#include <filesystem>
#include <optional>

namespace ssa::platform {

    struct LocalOpenPaths {
        std::filesystem::path inputFolder;
        std::filesystem::path processedFolder;
        std::filesystem::path redundantFolder;
        std::filesystem::path installationGuide;
    };

    class LocalOpenCommandHandler final {
      public:
        LocalOpenCommandHandler(LocalOpenPaths paths, OpenPathPolicy policy);

        [[nodiscard]] ports::ExternalCommandResult openPath(const std::string& path) const;
        [[nodiscard]] std::optional<ports::ExternalCommandResult>
        executeConfigured(ports::ExternalCommandKind kind) const;

      private:
        [[nodiscard]] ports::ExternalCommandResult
        openConfiguredPath(const std::filesystem::path& path) const;
        [[nodiscard]] std::optional<std::filesystem::path>
        configuredPathFor(ports::ExternalCommandKind kind) const;

        LocalOpenPaths paths_;
        OpenPathPolicy policy_;
    };

} // namespace ssa::platform
