#pragma once

#include "ports/IExternalCommandPort.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace ssa::platform {

    class OpenPathPolicy final {
      public:
        explicit OpenPathPolicy(std::vector<std::filesystem::path> allowedRoots);

        // Performs filesystem canonicalization; call from a background execution path.
        [[nodiscard]] ports::ExternalCommandResult validate(const std::string& rawPath) const;

      private:
        [[nodiscard]] static std::optional<std::filesystem::path>
        normalizePath(const std::filesystem::path& path);

        std::vector<std::filesystem::path> canonicalAllowedRoots_;
    };

} // namespace ssa::platform
