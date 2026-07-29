#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

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

    struct ApplicationLaunchTargets {
        std::filesystem::path databasePath;
        std::filesystem::path projectRoot;
        std::filesystem::path configDir;
    };

    class IApplicationLauncher {
      public:
        virtual ~IApplicationLauncher() = default;

        [[nodiscard]] virtual ApplicationLaunchResult
        launchWithDatabase(const std::filesystem::path& path) = 0;
        [[nodiscard]] virtual ApplicationLaunchResult
        launchConfigured(const ApplicationLaunchTargets& targets) = 0;
    };

    enum class DataSetupAction {
        CreateEmpty,
        ImportDatabase,
        ImportXlsx,
        ImportDatabaseAndXlsx,
    };

    struct DataSetupRequest {
        DataSetupAction action = DataSetupAction::CreateEmpty;
        std::filesystem::path projectRoot;
        std::filesystem::path sourceDatabase;
        std::vector<std::filesystem::path> xlsxFiles;
    };

    struct DataSetupResult {
        bool ok = false;
        std::string message;
        std::string diagnostic;
        std::filesystem::path databasePath;
    };

    enum class ExclusiveFilePublishStatus {
        Succeeded,
        DestinationExists,
        Failed,
    };

    struct ExclusiveFilePublishResult {
        ExclusiveFilePublishStatus status = ExclusiveFilePublishStatus::Failed;
        std::string diagnostic;
    };

    using ExclusiveFilePublisher = ExclusiveFilePublishResult(
        const std::filesystem::path& source, const std::filesystem::path& destination);

    class IDataSetupPort {
      public:
        virtual ~IDataSetupPort() = default;

        [[nodiscard]] virtual DataSetupResult execute(const DataSetupRequest& request,
                                                      std::stop_token stopToken = {}) = 0;
    };

} // namespace ssa::ports
