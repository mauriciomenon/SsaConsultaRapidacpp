#pragma once

#include "infra/SsaImportData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    struct SpreadsheetTable {
        std::filesystem::path sourcePath;
        std::vector<std::vector<std::string>> rows;
    };

} // namespace ssa::infra::importing
