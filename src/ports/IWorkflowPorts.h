#pragma once

#include "domain/SsaTypes.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::ports {

    enum class WorkflowStatus {
        Succeeded,
        NoChanges,
        Canceled,
        NotImplemented,
        Rejected,
        Failed,
    };

    enum class ImportFileStatus {
        Applied,
        NoChanges,
        NoValidRows,
        Rejected,
        Failed,
        Canceled,
    };

    struct ImportFileResult {
        std::string source;
        ImportFileStatus status = ImportFileStatus::Rejected;
        std::size_t validRows = 0;
        std::size_t invalidRows = 0;
        std::size_t invalidNumberRows = 0;
        std::size_t invalidDescriptionRows = 0;
        std::size_t invalidDateRows = 0;
        std::size_t inserts = 0;
        std::size_t updates = 0;
        std::size_t unchangedRows = 0;
        std::size_t conflicts = 0;
        bool consolidated = false;
        bool noSurvivor = false;
    };

    struct ImportSummary {
        std::size_t discovered = 0;
        std::size_t accepted = 0;
        std::size_t rejected = 0;
        std::size_t pending = 0;
        std::size_t preserved = 0;
        std::size_t validRows = 0;
        std::size_t invalidRows = 0;
        std::size_t invalidNumberRows = 0;
        std::size_t invalidDescriptionRows = 0;
        std::size_t invalidDateRows = 0;
        std::size_t skippedRows = 0;
        std::size_t duplicateRows = 0;
        std::size_t inserts = 0;
        std::size_t updates = 0;
        std::size_t unchangedRows = 0;
        std::size_t conflicts = 0;
        std::size_t consolidated = 0;
        std::size_t noSurvivor = 0;
        std::vector<ImportFileResult> files;
    };

    struct WorkflowResult {
        WorkflowStatus status = WorkflowStatus::NotImplemented;
        std::string message;
        bool warning = false;
        std::string diagnostic;
        std::optional<ImportSummary> importSummary;

        [[nodiscard]] bool ok() const {
            return status == WorkflowStatus::Succeeded || status == WorkflowStatus::NoChanges;
        }
    };

    struct ImportExecutionOptions final {
        static constexpr std::size_t kDefaultRowsPerChunk = 1'000;
        static constexpr std::size_t kMaxRowsPerChunk = 1'000;
        static constexpr auto kDefaultSqliteBusyWait = std::chrono::milliseconds{3'000};
        static constexpr auto kMaxSqliteBusyWait = std::chrono::milliseconds{3'000};
        static constexpr auto kSqliteBusyRetryGranularity = std::chrono::milliseconds{5};

        std::size_t rowsPerChunk{kDefaultRowsPerChunk};
        std::chrono::milliseconds sqliteBusyWait{kDefaultSqliteBusyWait};

        [[nodiscard]] std::string validationError() const {
            if (rowsPerChunk == 0 || rowsPerChunk > kMaxRowsPerChunk) {
                return "rows_per_chunk must be between 1 and 1000";
            }
            if (sqliteBusyWait.count() < 0 || sqliteBusyWait > kMaxSqliteBusyWait) {
                return "sqlite_busy_wait_ms must be between 0 and 3000";
            }
            if (sqliteBusyWait.count() % kSqliteBusyRetryGranularity.count() != 0) {
                return "sqlite_busy_wait_ms must be a multiple of 5";
            }
            return {};
        }
    };

    struct ImportExternalFilesRequest {
        std::vector<std::filesystem::path> files;
        ImportExecutionOptions execution;
    };

    struct ImportDerivationsRequest {
        std::vector<std::filesystem::path> files;
        ImportExecutionOptions execution;
    };

    struct SamArtifact {
        std::filesystem::path path;
        std::string executorSector;
        std::size_t manifestRows = 0;
        std::size_t detailRows = 0;
        std::size_t withoutDetailRows = 0;
    };

    struct SamImportRequest {
        std::vector<SamArtifact> artifacts;
        std::chrono::milliseconds sqliteBusyWait{ImportExecutionOptions::kDefaultSqliteBusyWait};
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
        std::chrono::milliseconds sqliteBusyWait{ImportExecutionOptions::kDefaultSqliteBusyWait};
    };

    struct SamFetchResult {
        WorkflowStatus status = WorkflowStatus::NotImplemented;
        std::string message;
        std::vector<SamArtifact> artifacts;
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
        ImportExecutionOptions execution;
    };

    struct ExportFilteredListRequest {
        domain::SsaPageRequest query;
        std::vector<std::string> headerLabels;
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

    class ISamImportPort {
      public:
        virtual ~ISamImportPort() = default;

        [[nodiscard]] virtual WorkflowResult importSamArtifacts(const SamImportRequest& request,
                                                                std::stop_token stopToken = {}) = 0;
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

        [[nodiscard]] virtual bool legacySpreadsheetConverterAvailable() const = 0;
        [[nodiscard]] virtual WorkflowResult
        importDerivations(const ImportDerivationsRequest& request,
                          std::stop_token stopToken = {}) = 0;
        [[nodiscard]] virtual WorkflowResult
        cleanOrphanDerivations(std::stop_token stopToken = {}) = 0;
    };

} // namespace ssa::ports
