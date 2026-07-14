#pragma once

#include <filesystem>
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
        std::size_t rowsUnchanged{0};
        std::size_t skippedRows{0};
        std::size_t duplicateRows{0};
        std::size_t conflictRows{0};
        std::size_t invalidRows{0};
        std::size_t invalidNumberRows{0};
        std::size_t invalidDescriptionRows{0};
        std::size_t invalidDateRows{0};
    };

    struct SsaImportBatchWriteSummary {
        std::size_t rowsWritten{0};
        std::size_t rowsInserted{0};
        std::size_t rowsUpdated{0};
        std::size_t rowsUnchanged{0};
        std::size_t conflictRows{0};
    };

    struct ResolvedSsaImportRows {
        std::vector<SsaImportRow> rows;
        std::size_t duplicateRows{0};
        std::size_t conflictRows{0};
    };

} // namespace ssa::infra::importing
