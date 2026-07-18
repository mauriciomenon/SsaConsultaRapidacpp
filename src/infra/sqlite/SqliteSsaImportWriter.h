#pragma once

#include "domain/ColumnCatalog.h"
#include "infra/SsaImportData.h"
#include "infra/import/ImportConsolidation.h"

#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::importing {
    class SpreadsheetImportWorkflowPort;
}

namespace ssa::infra::sqlite {

    class SqliteSsaImportWriterTestAccess;

    class SqliteSsaImportWriterAccess final {
      private:
        SqliteSsaImportWriterAccess() = default;

        friend class ssa::infra::importing::SpreadsheetImportWorkflowPort;
        friend class SqliteSsaImportWriterTestAccess;
    };

    class SqliteSsaImportWriter final {
      public:
        class WriteSession final {
          public:
            ~WriteSession();

            WriteSession(const WriteSession&) = delete;
            WriteSession& operator=(const WriteSession&) = delete;
            WriteSession(WriteSession&&) noexcept;
            WriteSession& operator=(WriteSession&&) noexcept;

            [[nodiscard]] importing::SsaImportBatchWriteSummary
            write(const importing::ResolvedSsaImportRows& rows, std::size_t fileCount,
                  std::size_t skippedRows);
            void recordConsolidation(const std::vector<importing::ImportConsolidationMove>& moves);
            [[nodiscard]] importing::SsaImportWriteSummary
            finishWithAnalytics(int observedIsoYearWeek, std::string observedDate);
            [[nodiscard]] importing::SsaImportWriteSummary finish();
            void rollback();

          private:
            friend class SqliteSsaImportWriter;

            struct Storage;

            explicit WriteSession(std::unique_ptr<Storage> storage);

            std::unique_ptr<Storage> storage_;
        };

        explicit SqliteSsaImportWriter(SqliteSsaImportWriterAccess,
                                       std::filesystem::path databasePath,
                                       std::vector<domain::ColumnDef> columns,
                                       std::string tableName = "ssa_table");

        // Convenience single-batch write. Multi-file imports should use WriteSession
        // to keep one transaction across batches.
        [[nodiscard]] importing::SsaImportWriteSummary
        write(const importing::ResolvedSsaImportRows& rows, std::size_t fileCount,
              std::size_t skippedRows, bool replaceAll, std::stop_token stopToken = {}) const;
        [[nodiscard]] WriteSession startSession(bool replaceAll,
                                                std::stop_token stopToken = {}) const;
        [[nodiscard]] std::vector<importing::ImportConsolidationMove>
        pendingConsolidation(const std::stop_token& stopToken = {}) const;
        void
        completeConsolidation(const std::vector<importing::ImportConsolidationMove>& moves) const;

      private:
        std::filesystem::path databasePath_;
        std::vector<domain::ColumnDef> columns_;
        std::string tableName_;
    };

} // namespace ssa::infra::sqlite
