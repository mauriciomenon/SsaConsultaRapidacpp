#pragma once

#include "domain/ColumnCatalog.h"

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
            MissingWeek,
            InvalidDate,
        };

        enum class SourceProfile {
            Geral,
            Executadas,
            DerivadasRelacionadas,
            Desvios,
        };

        struct MergeResult {
            Values values;
            bool changed = false;
            bool conflict = false;
            std::string conflictField; // first field that conflicted, for diagnostics
            // A row repeated inside the same workbook disagreed with its first row and was
            // dropped: SAM exports one row per execution, and the first row wins.
            bool repeatDiscarded = false;
        };

        [[nodiscard]] static std::string normalizeNumber(const std::string& value);
        [[nodiscard]] static std::string normalizeDeviationCount(const std::string& value);
        [[nodiscard]] static std::string normalizeDateText(const std::string& value);
        // Stored form of a date column value: DateTimeText is always "YYYY-MM-DD HH:MM:SS"
        // (Python parity), DateText keeps ISO as typed. Empty when the value is not a date.
        [[nodiscard]] static std::string normalizeDateValue(ColumnType type,
                                                            const std::string& value);
        [[nodiscard]] static std::string normalizeSnapshotTimestamp(const std::string& value);
        [[nodiscard]] static std::string normalizeFilenameTimestamp(const std::string& filename);
        [[nodiscard]] static bool isTerminalStatus(const std::string& status);
        [[nodiscard]] static SourceProfile classifySourceProfile(const std::string& filename);
        [[nodiscard]] static RowValidationIssue validateRow(const Values& row);
        [[nodiscard]] static bool isValidRow(const Values& row);
        [[nodiscard]] static MergeResult merge(const Values& existing, const Values& incoming);
        // One-line log text for a conflicting or discarded-repeat merge; values are clipped to
        // keep logs bounded.
        [[nodiscard]] static std::string
        describeConflict(const Values& existing, const Values& incoming, const MergeResult& result);
    };

} // namespace ssa::domain
