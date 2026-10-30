#pragma once

#include "domain/ColumnCatalog.h"

#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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
            // An open row is dated after a terminal (STE/SCA) row of the same SSA. The terminal
            // row still wins; real data never shows this, so it flags a stale file or a wrong
            // file date and is reported as a warning.
            bool openAfterTerminal = false;
        };

        [[nodiscard]] static std::string normalizeNumber(const std::string& value);
        // Empty text is null; nullopt is a numeric value that cannot become int64.
        [[nodiscard]] static std::optional<std::string>
        normalizeIntegerCount(const std::string& value);
        [[nodiscard]] static std::string normalizeExceededTime(const std::string& value);
        [[nodiscard]] static std::string normalizeDateText(const std::string& value);
        // Python UPSERT_DATE_COLUMNS, independent of the visual ColumnType.
        [[nodiscard]] static bool isUpsertDateColumn(std::string_view key);
        // Canonical storage timestamp; empty means null, including invalid non-empty text.
        [[nodiscard]] static std::string normalizeUpsertDate(const std::string& value);
        // Legacy type-based normalization for columns outside UPSERT_DATE_COLUMNS:
        // DateTimeText uses whole seconds; DateText keeps ISO as typed. Empty on invalid text.
        [[nodiscard]] static std::string normalizeDateValue(ColumnType type,
                                                            const std::string& value);
        [[nodiscard]] static std::string normalizeSnapshotTimestamp(const std::string& value);
        [[nodiscard]] static std::string normalizeFilenameTimestamp(const std::string& filename);
        [[nodiscard]] static bool isTerminalStatus(const std::string& status);
        // Total precedence of a row: its snapshot, then its file date. Empty when the row
        // has no date, so it sorts before any dated row.
        [[nodiscard]] static std::string precedenceKey(const Values& row);
        // Order of rows of equal precedence: more non-empty fields, then the larger
        // canonical serialization (Python json.dumps of the canonical row).
        struct ContentKey {
            std::size_t fields = 0;
            std::string json;
            auto operator<=>(const ContentKey&) const = default;
        };
        [[nodiscard]] static ContentKey contentKey(const Values& row);
        // An open row dated after a terminal (STE/SCA) one: a stale file or a wrong date.
        [[nodiscard]] static bool isOpenAfterTerminal(const Values& open, const Values& terminal);
        [[nodiscard]] static SourceProfile classifySourceProfile(const std::string& filename);
        [[nodiscard]] static RowValidationIssue validateRow(const Values& row);
        enum class ImportedRowIssue { None, InvalidNumber, InvalidReference, InvalidInteger };
        // Header presence is required: an absent row key can also mean an empty cell.
        [[nodiscard]] static ImportedRowIssue normalizeImportedCounts(Values& row,
                                                                      bool hasReprogrammingColumn);
        // Canonical numero_ssa, numero_desvios (dropped when not a count) and SSA references
        // (derivada_de, numero_ssa_relacionada_N) of an imported row, in place; every Integer
        // column of the catalog must then hold a 64-bit integer.
        [[nodiscard]] static ImportedRowIssue normalizeImportedRow(Values& row);
        // Trims every value and drops the Python null sentinels ("", "<na>", "none", "nan",
        // "null", "n/a", "-", case-insensitive): a null is an absent key, never "".
        static void dropNullValues(Values& row);
        // The whole value is a base-10 64-bit integer.
        [[nodiscard]] static bool parseInteger(std::string_view value, long long& parsed);
        [[nodiscard]] static bool isValidRow(const Values& row);
        [[nodiscard]] static MergeResult merge(const Values& existing, const Values& incoming);
        // One-line log text for a conflicting or discarded-repeat merge; values are clipped to
        // keep logs bounded.
        [[nodiscard]] static std::string
        describeConflict(const Values& existing, const Values& incoming, const MergeResult& result);
    };

} // namespace ssa::domain
