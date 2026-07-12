#pragma once

#include "ports/IExternalCommandPort.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace ssa::platform {

    class OpenPathPolicy final {
      public:
        explicit OpenPathPolicy(std::vector<std::filesystem::path> allowedRoots);

        // Performs filesystem canonicalization; call from a background execution path.
        [[nodiscard]] ports::ExternalCommandResult
        validate(const std::filesystem::path& rawPath) const;

      private:
        [[nodiscard]] static std::optional<std::filesystem::path>
        canonicalizePath(const std::filesystem::path& path);

        std::vector<std::filesystem::path> canonicalAllowedRoots_;
        std::size_t rejectedAllowedRoots_{0};
    };

} // namespace ssa::platform
