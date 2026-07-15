#pragma once

#include "infra/import/ImportFileStager.h"
#include "infra/import/SamSpreadsheetAdapter.h"
#include "infra/import/SsaImportConflictResolver.h"
#include "infra/import/SsaSpreadsheetMapper.h"
#include "infra/import/XlsxWorkbookReader.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "ports/IWorkflowPorts.h"

#include <optional>

namespace ssa::infra::importing {

    class SpreadsheetImportWorkflowPort final : public ports::IImportWorkflowPort,
                                                public ports::ISamImportPort {
      public:
        SpreadsheetImportWorkflowPort(std::filesystem::path inputFolder,
                                      std::filesystem::path databasePath,
                                      std::vector<domain::ColumnDef> columns);

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
                              const std::vector<ports::SamArtifact>* samArtifacts = nullptr) const;
        [[nodiscard]] ports::WorkflowResult
        importIncrementalFiles(const ImportStagingResult& files,
                               const std::stop_token& stopToken) const;
        [[nodiscard]] std::optional<ports::WorkflowResult>
        resumePendingConsolidation(const std::stop_token& stopToken) const;

        std::filesystem::path importLockPath_;
        std::filesystem::path databaseImportLockPath_;
        ImportFileStager stager_;
        XlsxWorkbookReader reader_;
        SsaSpreadsheetMapper mapper_;
        SsaImportConflictResolver conflictResolver_;
        sqlite::SqliteSsaImportWriter writer_;
    };

} // namespace ssa::infra::importing
