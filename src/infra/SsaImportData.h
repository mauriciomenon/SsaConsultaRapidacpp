#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssa::infra::importing {

    using SsaImportRow = std::unordered_map<std::string, std::string>;

    [[nodiscard]] inline std::string rowValue(const SsaImportRow& row, const std::string& key) {
        const auto found = row.find(key);
        return found == row.end() ? std::string{} : found->second;
    }

    [[nodiscard]] inline std::string rowValue(const SsaImportRow& row, const char* key) {
        const auto found = row.find(key);
        return found == row.end() ? std::string{} : found->second;
    }

    enum class SpreadsheetMappingStatus {
        HeaderNotRecognized,
        RequiredColumnsMissing,
        AmbiguousHeaders,
        Mapped
    };

    struct SsaImportBatch {
        std::filesystem::path sourcePath;
        std::vector<std::string> headerRow;
        std::vector<SsaImportRow> rows;
        std::size_t skippedRows{0};
        std::size_t invalidRows{0};
        std::size_t invalidNumberRows{0};
        std::size_t invalidDescriptionRows{0};
        std::size_t invalidDateRows{0};
        std::size_t mappedColumns{0};
        SpreadsheetMappingStatus mappingStatus{SpreadsheetMappingStatus::HeaderNotRecognized};
    };

    struct SsaImportWriteSummary {
        std::size_t files{0};
        std::size_t rowsWritten{0};
        std::size_t rowsInserted{0};
        std::size_t rowsUpdated{0};
        std::size_t rowsVerified{0};
        std::size_t rowsUnchanged{0};
        std::size_t skippedRows{0};
        std::size_t duplicateRows{0};
        std::size_t conflictRows{0};
        std::size_t invalidRows{0};
        std::size_t invalidNumberRows{0};
        std::size_t invalidDescriptionRows{0};
        std::size_t invalidDateRows{0};
        std::size_t openAfterTerminalRows{0};
    };

    struct SsaImportRowCounts {
        std::size_t rowsInserted{0};
        std::size_t rowsUpdated{0};
        std::size_t rowsVerified{0};
        std::size_t rowsUnchanged{0};
    };

    struct SsaImportBatchWriteSummary {
        // Row categories by the source that won each SSA (ResolvedSsaImportRows::sources).
        std::map<std::size_t, SsaImportRowCounts> bySource;
        std::size_t rowsWritten{0};
        std::size_t rowsInserted{0};
        std::size_t rowsUpdated{0};
        std::size_t rowsVerified{0};
        std::size_t rowsUnchanged{0};
        std::size_t duplicateRows{0};
        std::size_t conflictRows{0};
    };

    struct ResolvedSsaImportRows {
        std::vector<SsaImportRow> rows;
        // Aligned with rows: the source that won the SSA and the dating fields of its newest
        // open row (empty when none), compared with a terminal row stored in the database.
        std::vector<std::size_t> sources;
        std::vector<SsaImportRow> newestOpen;
        // Dating fields of the oldest terminal row of the run (empty when none), compared with
        // an open row stored in the database.
        std::vector<SsaImportRow> oldestTerminal;
        std::size_t duplicateRows{0};
        std::size_t conflictRows{0};
        std::size_t openAfterTerminalRows{0};
    };

} // namespace ssa::infra::importing
