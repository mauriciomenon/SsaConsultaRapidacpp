#pragma once

#include "domain/ColumnCatalog.h"
#include "infra/SsaImportData.h"

#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::sqlite {

    class SqliteSsaImportWriter final {
      public:
        class WriteSession final {
          public:
            ~WriteSession();

            WriteSession(const WriteSession&) = delete;
            WriteSession& operator=(const WriteSession&) = delete;
            WriteSession(WriteSession&&) noexcept;
            WriteSession& operator=(WriteSession&&) noexcept;

            void write(const importing::ResolvedSsaImportRows& rows, std::size_t fileCount,
                       std::size_t skippedRows);
            [[nodiscard]] importing::SsaImportWriteSummary finish();
            void rollback();

          private:
            friend class SqliteSsaImportWriter;

            struct Storage;

            explicit WriteSession(std::unique_ptr<Storage> storage);

            std::unique_ptr<Storage> storage_;
        };

        explicit SqliteSsaImportWriter(std::filesystem::path databasePath,
                                       std::vector<domain::ColumnDef> columns,
                                       std::string tableName = "ssa_table");

        // Convenience single-batch write. Multi-file imports should use WriteSession
        // to keep one transaction across batches.
        [[nodiscard]] importing::SsaImportWriteSummary
        write(const importing::ResolvedSsaImportRows& rows, std::size_t fileCount,
              std::size_t skippedRows, bool replaceAll, std::stop_token stopToken = {}) const;
        [[nodiscard]] WriteSession startSession(bool replaceAll,
                                                std::stop_token stopToken = {}) const;

      private:
        std::filesystem::path databasePath_;
        std::vector<domain::ColumnDef> columns_;
        std::string tableName_;
    };

} // namespace ssa::infra::sqlite
