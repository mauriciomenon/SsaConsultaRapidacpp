#pragma once

#include "infra/SsaImportData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    struct RawCell {
        enum class Kind { Missing, String, Number, Boolean, Date, DateTime, Time };
        Kind kind = Kind::Missing;
        // Python scalar spelling, before trimming or database conversions.
        std::string value;
    };

    struct SpreadsheetTable {
        std::filesystem::path sourcePath;
        std::string originalFilename;
        std::string sourceModifiedTimestamp;
        std::vector<std::vector<std::string>> rows;
        // A continuation chunk may carry the header resolved from its first chunk.
        // Keeping it outside rows avoids shifting every row on each chunk.
        std::vector<std::string> headerRow;
        std::string sourceCreatedTimestamp;
        // Same row/column positions as rows; omitted cells in a sheet are empty strings.
        std::vector<std::vector<RawCell>> rawRows;
    };

} // namespace ssa::infra::importing
