#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    class XlsxWorkbookReader final {
      public:
        [[nodiscard]] static SpreadsheetTable readFirstSheet(const std::filesystem::path& filePath,
                                                             const std::stop_token& stopToken = {});

      private:
        struct WorksheetRelationshipRequest {
            const std::string& xml;
            const std::string& relationshipId;
        };

        [[nodiscard]] static std::vector<std::string>
        parseSharedStrings(const std::string& xml, const std::stop_token& stopToken);
        [[nodiscard]] static std::string
        relationshipIdForFirstWorkbookSheet(const std::string& xml,
                                            const std::stop_token& stopToken);
        [[nodiscard]] static std::string
        worksheetEntryForRelationship(const WorksheetRelationshipRequest& request,
                                      const std::stop_token& stopToken);
        [[nodiscard]] static std::vector<std::vector<std::string>>
        parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings,
                       const std::vector<bool>& dateStyles, bool date1904,
                       const std::stop_token& stopToken);
    };

} // namespace ssa::infra::importing
