#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <system_error>

namespace ssa::infra::importing {

    struct SourceFileSnapshot {
        std::string identity;
        std::uintmax_t size = 0;
        std::chrono::sys_time<std::chrono::seconds> modified;
        std::chrono::nanoseconds modifiedSubseconds{};
        std::optional<std::chrono::sys_seconds> created;

        [[nodiscard]] bool operator==(const SourceFileSnapshot&) const = default;
        [[nodiscard]] std::string diagnostic() const;
        [[nodiscard]] std::filesystem::file_time_type fileModificationTime() const;
        [[nodiscard]] std::string modifiedTimestamp() const;
        [[nodiscard]] std::string createdTimestamp() const;
    };

    class SourceReadDeferred final : public std::runtime_error {
      public:
        SourceReadDeferred(std::string reason, const std::string& detail);
        [[nodiscard]] const std::string& reason() const noexcept;

      private:
        std::string reason_;
    };

    // The consumer owns each copied chunk before returning. No source handle escapes.
    [[nodiscard]] SourceFileSnapshot
    captureSourceFile(const std::filesystem::path& path,
                      const std::function<void(std::span<const char>)>& consume,
                      std::stop_token stopToken = {},
                      std::uintmax_t maxBytes = 128ULL * 1024ULL * 1024ULL,
                      const std::function<void()>& afterFirstChunkRead = {});

    [[nodiscard]] std::optional<SourceFileSnapshot>
    inspectSourceFileIdentity(const std::filesystem::path& path, std::error_code& error);

} // namespace ssa::infra::importing
