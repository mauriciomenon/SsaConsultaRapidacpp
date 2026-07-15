#pragma once

#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::ports {

    enum class ExternalProcessStatus {
        Succeeded,
        Canceled,
        TimedOut,
        StartFailed,
        Failed,
        FailedToStop,
    };

    struct ExternalProcessRequest {
        std::filesystem::path program;
        std::vector<std::string> arguments;
        std::filesystem::path workingDirectory;
        std::chrono::milliseconds timeout{180'000};
    };

    struct ExternalProcessResult {
        ExternalProcessStatus status = ExternalProcessStatus::Failed;
        int exitCode = -1;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == ExternalProcessStatus::Succeeded;
        }
    };

    class IExternalProcessRunner {
      public:
        virtual ~IExternalProcessRunner() = default;

        [[nodiscard]] virtual ExternalProcessResult
        run(const ExternalProcessRequest& request, const std::stop_token& stopToken = {}) const = 0;
    };

} // namespace ssa::ports
