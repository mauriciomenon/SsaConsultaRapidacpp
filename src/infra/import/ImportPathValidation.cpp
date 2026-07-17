#include "infra/import/ImportPathValidation.h"

#include <algorithm>
#include <string_view>
#include <system_error>

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxDiagnosticBytes = 4'096;

        void appendDiagnostic(std::string& destination, const std::string_view diagnostic) {
            if (diagnostic.empty() || destination.size() >= kMaxDiagnosticBytes) {
                return;
            }
            if (!destination.empty()) {
                destination += "; ";
            }
            destination.append(diagnostic.substr(
                0, (std::min)(diagnostic.size(), kMaxDiagnosticBytes - destination.size())));
        }

    } // namespace

    bool isSafeImportFilename(const std::filesystem::path& filename) {
        return !filename.empty() && filename == filename.filename() && filename != "." &&
               filename != "..";
    }

    std::string inputDirectoryRejectionReason(const std::filesystem::path& directory,
                                              std::string& diagnostic) {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(directory, error);
        if (error == std::errc::no_such_file_or_directory) {
            return {};
        }
        if (error) {
            appendDiagnostic(diagnostic, "cannot inspect input directory: " + error.message());
            return "input_directory_status_unavailable";
        }
        if (std::filesystem::is_symlink(status)) {
            return "input_directory_symlink";
        }
        if (std::filesystem::exists(status) && !std::filesystem::is_directory(status)) {
            return "input_directory_not_directory";
        }
        return {};
    }

} // namespace ssa::infra::importing
