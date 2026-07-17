#pragma once

#include <filesystem>
#include <string>

namespace ssa::infra::importing {

    [[nodiscard]] bool isSafeImportFilename(const std::filesystem::path& filename);
    [[nodiscard]] std::string inputDirectoryRejectionReason(const std::filesystem::path& directory,
                                                            std::string& diagnostic);

} // namespace ssa::infra::importing
