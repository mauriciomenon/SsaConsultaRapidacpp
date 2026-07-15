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
        std::string sourceCreatedTimestamp;
    };

} // namespace ssa::infra::importing
