#pragma once

#include "infra/import/ImportFileConsolidator.h"
#include "infra/import/ImportFileStager.h"
#include "infra/import/SamSpreadsheetAdapter.h"
#include "infra/import/SsaImportConflictResolver.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "ports/IWorkflowPorts.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    class SpreadsheetImportWorkflowPort final : public ports::IImportWorkflowPort,
                                                public ports::ISamImportPort {
      public:
        using SynchronizationSemaphore = sqlite::SqliteSsaImportWriter::SynchronizationSemaphore;

        struct SynchronizationSignals {
            std::shared_ptr<SynchronizationSemaphore> writerBusyEntered;
            std::shared_ptr<SynchronizationSemaphore> snapshotLocked;
            FileCopyFirstChunkWrittenHook afterFirstChunkWritten;
        };

        SpreadsheetImportWorkflowPort(std::filesystem::path inputFolder,
                                      std::filesystem::path databasePath,
                                      std::vector<domain::ColumnDef> columns,
                                      bool consolidateSources = true,
                                      SynchronizationSignals synchronization = {});

        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request,
                            std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult
        importSamArtifacts(const ports::SamImportRequest& request,
                           std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request,
                                                   std::stop_token stopToken = {}) override;

      private:
        struct ProgressContext {
            const ports::WorkflowProgressCallback* callback = nullptr;
            std::size_t fileOffset = 0;
            std::size_t totalFiles = 0;
            std::size_t batchIndex = 0;
            bool rescan = false;
        };

        [[nodiscard]] ports::WorkflowResult
        importDiscoveredFiles(const ImportStagingResult& files, bool replaceAll,
                              const std::stop_token& stopToken,
                              const ports::ImportExecutionOptions& execution,
                              const std::vector<ports::SamArtifact>* samArtifacts = nullptr,
                              const ProgressContext* progress = nullptr) const;
        [[nodiscard]] ports::WorkflowResult
        importExternalFilesBatch(const ports::ImportExternalFilesRequest& request,
                                 const std::stop_token& stopToken,
                                 const ProgressContext& progress) const;
        [[nodiscard]] ports::WorkflowResult rescanInternal(const ports::RescanRequest& request,
                                                           const std::stop_token& stopToken,
                                                           ProgressContext& progress);
        static void reportProgress(const ProgressContext& context,
                                   ports::WorkflowProgressStage stage,
                                   ports::WorkflowProgressLevel level, std::size_t currentFile,
                                   int percentage, std::string status, std::string detail = {},
                                   std::string fileName = {}) noexcept;
        [[nodiscard]] std::optional<ports::WorkflowResult> resumePendingConsolidation(
            const std::stop_token& stopToken, std::chrono::milliseconds sqliteBusyWait,
            const ImportStagingResult* selectedStaging = nullptr,
            std::vector<std::size_t>* selectedPendingSummaryIndices = nullptr) const;

        std::filesystem::path inputFolder_;
        std::filesystem::path databasePath_;
        std::vector<domain::ColumnDef> columns_;
        bool consolidateSources_{true};
        std::filesystem::path importLockPath_;
        std::string importLockPathDiagnostic_;
        ImportFileStager stager_;
        ImportFileConsolidator consolidator_;
        SsaImportConflictResolver conflictResolver_;
        SynchronizationSignals synchronization_;
        sqlite::SqliteSsaImportWriter writer_;
    };

} // namespace ssa::infra::importing
