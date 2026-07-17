#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>

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

    struct FileIdentitySnapshot {
        std::string value;
        std::uintmax_t size = 0;
    };

    [[nodiscard]] std::optional<FileIdentitySnapshot>
    inspectFileIdentity(const std::filesystem::path& path, std::error_code& error);

    [[nodiscard]] FileCopyResult copyFileAtomically(const FileCopyRequest& request,
                                                    std::stop_token stopToken = {});

} // namespace ssa::infra::importing
