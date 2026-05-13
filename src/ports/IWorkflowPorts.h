#pragma once

#include "domain/SsaTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::ports {

    enum class WorkflowStatus {
        Succeeded,
        NotImplemented,
        Rejected,
        Failed,
    };

    struct WorkflowResult {
        WorkflowStatus status{WorkflowStatus::NotImplemented};
        std::string message;

        [[nodiscard]] bool ok() const {
            return status == WorkflowStatus::Succeeded;
        }
    };

    struct ImportExternalFilesRequest {
        std::vector<std::filesystem::path> files;
        bool optimized{true};
    };

    enum class RescanMode {
        Incremental,
        Full,
    };

    struct RescanRequest {
        RescanMode mode{RescanMode::Incremental};
        bool allowFileDiscovery{true};
        bool optimized{true};
    };

    struct ExportFilteredListRequest {
        domain::SsaPageRequest query;
        std::filesystem::path outputPath;
    };

    class IImportWorkflowPort {
      public:
        virtual ~IImportWorkflowPort() = default;

        [[nodiscard]] virtual WorkflowResult
        importExternalFiles(const ImportExternalFilesRequest& request) = 0;
        [[nodiscard]] virtual WorkflowResult rescan(const RescanRequest& request) = 0;
    };

    class IExportPort {
      public:
        virtual ~IExportPort() = default;

        [[nodiscard]] virtual WorkflowResult
        exportFilteredList(const ExportFilteredListRequest& request) = 0;
    };

    class IDatabaseMaintenancePort {
      public:
        virtual ~IDatabaseMaintenancePort() = default;

        [[nodiscard]] virtual WorkflowResult resetDatabase() = 0;
        [[nodiscard]] virtual WorkflowResult cleanData() = 0;
        [[nodiscard]] virtual WorkflowResult vacuumAnalyze() = 0;
    };

    class IDerivadasPort {
      public:
        virtual ~IDerivadasPort() = default;

        [[nodiscard]] virtual WorkflowResult syncDerivadas() = 0;
    };

} // namespace ssa::ports
