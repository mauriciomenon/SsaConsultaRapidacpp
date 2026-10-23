#pragma once

#include <filesystem>
#include <string>

namespace ssa::infra::importing {

    [[nodiscard]] bool isSafeImportFilename(const std::filesystem::path& filename);
    // ASCII-lowercased extension in UTF-8 (path::string() would use the ANSI code page on
    // Windows and throw for characters outside it).
    [[nodiscard]] std::string lowercaseExtension(const std::filesystem::path& path);
    [[nodiscard]] std::string inputDirectoryRejectionReason(const std::filesystem::path& directory,
                                                            std::string& diagnostic);

} // namespace ssa::infra::importing
