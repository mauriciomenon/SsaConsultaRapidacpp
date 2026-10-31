#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <functional>
#include <stop_token>

namespace ssa::infra::importing {

    class SsaSpreadsheetMapper final {
      public:
        using MappingCheckpoint = std::function<void()>;

        struct FileMetadata {
            std::string sourceFilename;
            std::string sourceTimestamp;
            std::string sheetTimestamp;
            std::string createdTimestamp;
            bool legacySummary = false;
        };

        [[nodiscard]] static FileMetadata fileMetadata(const SpreadsheetTable& table);
        [[nodiscard]] static SsaImportBatch map(SpreadsheetTable table,
                                                const std::stop_token& stopToken = {});
        [[nodiscard]] static SsaImportBatch map(SpreadsheetTable table,
                                                const std::stop_token& stopToken,
                                                const MappingCheckpoint& afterFirstRowMapped);
        [[nodiscard]] static SsaImportBatch map(SpreadsheetTable table,
                                                const FileMetadata& metadata,
                                                const std::stop_token& stopToken,
                                                const MappingCheckpoint& afterFirstRowMapped = {});
    };

} // namespace ssa::infra::importing
