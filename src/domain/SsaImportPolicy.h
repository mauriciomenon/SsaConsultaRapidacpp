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
            std::string conflictField; // first differing field, for diagnostics
            // Two rows of equal precedence disagreed on a business field and were resolved by
            // content (more non-empty fields, then the larger canonical serialization), never
            // by read order. Journaled; `conflict` is never set by merge().
            bool repeatDiscarded = false;
            // Only source metadata (arquivo_origem, data_arquivo_origem, data_criacao_arquivo,
            // data_planilha) differs: the row is rewritten but counted as verified, not updated.
            bool metadataOnly = false;
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
