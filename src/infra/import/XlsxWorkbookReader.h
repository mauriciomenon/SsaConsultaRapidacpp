#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    class XlsxWorkbookReader final {
      public:
        using SheetChunkConsumer =
            std::function<void(SpreadsheetTable, bool firstInSheet, bool lastInSheet)>;

        [[nodiscard]] static SpreadsheetTable readFirstSheet(const std::filesystem::path& filePath,
                                                             const std::stop_token& stopToken = {});
        [[nodiscard]] static std::vector<SpreadsheetTable>
        readSheets(const std::filesystem::path& filePath, const std::stop_token& stopToken = {});
        static void readSheetChunks(const std::filesystem::path& filePath, std::size_t rowsPerChunk,
                                    const SheetChunkConsumer& consume,
                                    const std::stop_token& stopToken = {});

      private:
        struct WorksheetRelationshipRequest {
            const std::string& xml;
            const std::string& relationshipId;
        };

        [[nodiscard]] static std::vector<std::string>
        parseSharedStrings(const std::string& xml, const std::stop_token& stopToken);
        [[nodiscard]] static std::vector<std::string>
        relationshipIdsForWorkbookSheets(const std::string& xml, const std::stop_token& stopToken);
        [[nodiscard]] static std::string
        worksheetEntryForRelationship(const WorksheetRelationshipRequest& request,
                                      const std::stop_token& stopToken);
        static void parseSheetRowChunks(const std::string& xml,
                                        const std::vector<std::string>& sharedStrings,
                                        const std::vector<bool>& dateStyles, bool date1904,
                                        std::size_t rowsPerChunk, const SheetChunkConsumer& consume,
                                        const SpreadsheetTable& metadata,
                                        const std::stop_token& stopToken);
    };

} // namespace ssa::infra::importing
