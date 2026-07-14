#pragma once

#include <filesystem>
#include <stop_token>
#include <string>

namespace ssa::infra::importing {

    enum class FileCopyStatus {
        Succeeded,
        Canceled,
        CleanupFailed,
        Failed,
    };

    struct FileCopyResult {
        FileCopyStatus status = FileCopyStatus::Failed;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == FileCopyStatus::Succeeded;
        }
    };

    struct FileCopyRequest {
        const std::filesystem::path& source;
        const std::filesystem::path& destination;
    };

    [[nodiscard]] FileCopyResult copyFileAtomically(const FileCopyRequest& request,
                                                    std::stop_token stopToken = {});

} // namespace ssa::infra::importing
