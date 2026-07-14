#pragma once

#include <string>
#include <unordered_map>

namespace ssa::domain {

    class SsaImportPolicy final {
      public:
        using Values = std::unordered_map<std::string, std::string>;

        enum class RowValidationIssue {
            None,
            InvalidNumber,
            MissingDescription,
            MissingDate,
            InvalidDate,
        };

        struct MergeResult {
            Values values;
            bool changed = false;
            bool conflict = false;
        };

        [[nodiscard]] static std::string normalizeNumber(const std::string& value);
        [[nodiscard]] static std::string normalizeSnapshotTimestamp(const std::string& value);
        [[nodiscard]] static std::string normalizeFilenameTimestamp(const std::string& filename);
        [[nodiscard]] static RowValidationIssue validateRow(const Values& row);
        [[nodiscard]] static bool isValidRow(const Values& row);
        [[nodiscard]] static MergeResult merge(const Values& existing, const Values& incoming);
    };

} // namespace ssa::domain
