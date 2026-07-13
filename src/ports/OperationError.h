#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace ssa::ports {

    class OperationError final : public std::runtime_error {
      public:
        OperationError(const char* safeMessage, std::string diagnostic)
            : std::runtime_error(safeMessage), diagnostic_(std::move(diagnostic)) {}

        [[nodiscard]] const std::string& diagnostic() const noexcept {
            return diagnostic_;
        }

      private:
        std::string diagnostic_;
    };

} // namespace ssa::ports
