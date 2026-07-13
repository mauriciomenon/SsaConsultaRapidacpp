#pragma once

#include "domain/SsaTypes.h"

#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::ports {

    enum class WorkflowStatus {
        Succeeded,
        Canceled,
        NotImplemented,
        Rejected,
        Failed,
    };

    struct WorkflowResult {
        WorkflowStatus status = WorkflowStatus::NotImplemented;
        std::string message;
        bool warning = false;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == WorkflowStatus::Succeeded;
        }
    };

    struct ImportExternalFilesRequest {
        std::vector<std::filesystem::path> files;
        bool optimized = true;
    };

    struct SamRefreshRequest {
        bool enabled = false;
        std::filesystem::path scrapReportRoot;
        std::filesystem::path caFile;
        std::string baseUrl;
        std::vector<std::string> executorSectors;
        std::string scope = "consulta";
        int intervalMinutes = 30'000;
        std::chrono::milliseconds processTimeout{180'000};
    };

    struct SamFetchResult {
        WorkflowStatus status = WorkflowStatus::NotImplemented;
        std::string message;
        std::vector<std::filesystem::path> artifacts;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == WorkflowStatus::Succeeded;
        }
    };

    enum class RescanMode {
        Incremental,
        Full,
    };

    struct RescanRequest {
        RescanMode mode = RescanMode::Incremental;
        bool allowFileDiscovery = true;
        bool optimized = true;
    };

    struct ExportFilteredListRequest {
        domain::SsaPageRequest query;
        std::filesystem::path outputPath;
    };

    class IImportWorkflowPort {
      public:
        virtual ~IImportWorkflowPort() = default;

        [[nodiscard]] virtual WorkflowResult
        importExternalFiles(const ImportExternalFilesRequest& request,
                            std::stop_token stopToken = {}) = 0;
        [[nodiscard]] virtual WorkflowResult rescan(const RescanRequest& request,
                                                    std::stop_token stopToken = {}) = 0;
    };

    class ISamRefreshPort {
      public:
        virtual ~ISamRefreshPort() = default;

        [[nodiscard]] virtual SamFetchResult fetch(const SamRefreshRequest& request,
                                                   std::stop_token stopToken = {}) = 0;
        virtual bool discardArtifacts() = 0;
    };

    class IExportPort {
      public:
        virtual ~IExportPort() = default;

        [[nodiscard]] virtual WorkflowResult
        exportFilteredList(const ExportFilteredListRequest& request,
                           std::stop_token stopToken = {}) = 0;
    };

    class IDatabaseMaintenancePort {
      public:
        virtual ~IDatabaseMaintenancePort() = default;

        [[nodiscard]] virtual WorkflowResult resetDatabase(std::stop_token stopToken = {}) = 0;
        [[nodiscard]] virtual WorkflowResult cleanData(std::stop_token stopToken = {}) = 0;
        [[nodiscard]] virtual WorkflowResult vacuumAnalyze(std::stop_token stopToken = {}) = 0;
    };

    class IDerivadasPort {
      public:
        virtual ~IDerivadasPort() = default;

        [[nodiscard]] virtual WorkflowResult syncDerivadas(std::stop_token stopToken = {}) = 0;
    };

} // namespace ssa::ports
