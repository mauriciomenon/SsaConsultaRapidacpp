#pragma once

#include "infra/import/ImportFileStager.h"
#include "infra/import/SsaImportConflictResolver.h"
#include "infra/import/SsaSpreadsheetMapper.h"
#include "infra/import/XlsxWorkbookReader.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "ports/IWorkflowPorts.h"

namespace ssa::infra::importing {

    class SpreadsheetImportWorkflowPort final : public ports::IImportWorkflowPort {
      public:
        SpreadsheetImportWorkflowPort(std::filesystem::path inputFolder,
                                      std::filesystem::path databasePath,
                                      std::vector<domain::ColumnDef> columns,
                                      LegacySpreadsheetConverter legacyConverter = {});

        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request) override;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request) override;

      private:
        [[nodiscard]] ports::WorkflowResult importDiscoveredFiles(const ImportStagingResult& files,
                                                                  bool replaceAll) const;

        ImportFileStager stager_;
        XlsxWorkbookReader reader_;
        SsaSpreadsheetMapper mapper_;
        SsaImportConflictResolver conflictResolver_;
        sqlite::SqliteSsaImportWriter writer_;
    };

} // namespace ssa::infra::importing
