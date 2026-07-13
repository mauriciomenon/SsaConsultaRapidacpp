#pragma once

#include <filesystem>
#include <stop_token>
#include <string>

namespace ssa::ports {

    enum class DatabaseValidationStatus { Valid, Invalid, Canceled, Failed };

    struct DatabaseValidationResult {
        DatabaseValidationStatus status{DatabaseValidationStatus::Invalid};
        std::string message;
        std::string diagnostic;

        [[nodiscard]] bool valid() const {
            return status == DatabaseValidationStatus::Valid;
        }
    };

    class IDatabaseValidator {
      public:
        virtual ~IDatabaseValidator() = default;

        [[nodiscard]] virtual DatabaseValidationResult
        validate(const std::filesystem::path& path, std::stop_token stopToken = {}) const = 0;
    };

    struct ApplicationLaunchResult {
        bool started = false;
        std::string message;
    };

    class IApplicationLauncher {
      public:
        virtual ~IApplicationLauncher() = default;

        [[nodiscard]] virtual ApplicationLaunchResult
        launchWithDatabase(const std::filesystem::path& path) = 0;
    };

} // namespace ssa::ports
