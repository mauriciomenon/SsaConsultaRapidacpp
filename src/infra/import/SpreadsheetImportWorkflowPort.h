#pragma once

#include "infra/import/ImportFileConsolidator.h"
#include "infra/import/ImportFileStager.h"
#include "infra/import/SamSpreadsheetAdapter.h"
#include "infra/import/SsaImportConflictResolver.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "ports/IWorkflowPorts.h"

#include <optional>
#include <string>

namespace ssa::infra::importing {

    class SpreadsheetImportWorkflowPort final : public ports::IImportWorkflowPort,
                                                public ports::ISamImportPort {
      public:
        SpreadsheetImportWorkflowPort(std::filesystem::path inputFolder,
                                      std::filesystem::path databasePath,
                                      std::vector<domain::ColumnDef> columns,
                                      bool consolidateSources = true);

        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request,
                            std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult
        importSamArtifacts(const ports::SamImportRequest& request,
                           std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request,
                                                   std::stop_token stopToken = {}) override;

      private:
        [[nodiscard]] ports::WorkflowResult
        importDiscoveredFiles(const ImportStagingResult& files, bool replaceAll,
                              const std::stop_token& stopToken,
                              const ports::ImportExecutionOptions& execution,
                              const std::vector<ports::SamArtifact>* samArtifacts = nullptr) const;
        [[nodiscard]] ports::WorkflowResult
        importIncrementalFiles(const ImportStagingResult& files, const std::stop_token& stopToken,
                               const ports::ImportExecutionOptions& execution) const;
        [[nodiscard]] std::optional<ports::WorkflowResult>
        resumePendingConsolidation(const std::stop_token& stopToken,
                                   std::chrono::milliseconds sqliteBusyWait) const;

        std::filesystem::path inputFolder_;
        std::filesystem::path databasePath_;
        std::vector<domain::ColumnDef> columns_;
        bool consolidateSources_{true};
        std::filesystem::path importLockPath_;
        std::string importLockPathDiagnostic_;
        ImportFileStager stager_;
        ImportFileConsolidator consolidator_;
        SsaImportConflictResolver conflictResolver_;
        sqlite::SqliteSsaImportWriter writer_;
    };

} // namespace ssa::infra::importing
