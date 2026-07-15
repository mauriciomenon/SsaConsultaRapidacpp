#pragma once

#include "infra/SsaImportData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    struct SpreadsheetTable {
        std::filesystem::path sourcePath;
        std::string originalFilename;
        std::string sourceModifiedTimestamp;
        std::vector<std::vector<std::string>> rows;
        // A continuation chunk may carry the header resolved from its first chunk.
        // Keeping it outside rows avoids shifting every row on each chunk.
        std::vector<std::string> headerRow;
        std::string sourceCreatedTimestamp;
    };

} // namespace ssa::infra::importing
