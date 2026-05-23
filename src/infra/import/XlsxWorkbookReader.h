#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    class XlsxWorkbookReader final {
      public:
        [[nodiscard]] SpreadsheetTable readFirstSheet(const std::filesystem::path& filePath) const;

      private:
        [[nodiscard]] static std::vector<std::string> parseSharedStrings(const std::string& xml);
        [[nodiscard]] static std::string
        relationshipIdForFirstWorkbookSheet(const std::string& xml);
        [[nodiscard]] static std::string
        worksheetEntryForRelationship(const std::string& xml, const std::string& relationshipId);
        [[nodiscard]] static std::vector<std::vector<std::string>>
        parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings);
    };

} // namespace ssa::infra::importing
