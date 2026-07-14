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

    enum class SpreadsheetMappingStatus { HeaderNotRecognized, Mapped };

    struct SsaImportBatch {
        std::filesystem::path sourcePath;
        std::vector<SsaImportRow> rows;
        std::size_t skippedRows{0};
        std::size_t mappedColumns{0};
        SpreadsheetMappingStatus mappingStatus{SpreadsheetMappingStatus::HeaderNotRecognized};
    };

    struct SsaImportWriteSummary {
        std::size_t files{0};
        std::size_t rowsWritten{0};
        std::size_t skippedRows{0};
        std::size_t duplicateRows{0};
    };

    struct ResolvedSsaImportRows {
        std::vector<SsaImportRow> rows;
        std::vector<std::string> ssaNumbersForUpsertDelete;
        std::size_t duplicateRows{0};
    };

} // namespace ssa::infra::importing
