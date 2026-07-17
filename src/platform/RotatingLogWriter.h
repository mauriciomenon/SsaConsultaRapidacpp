#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace ssa::platform {

    class RotatingLogWriter final {
      public:
        RotatingLogWriter(std::filesystem::path filePath, std::uintmax_t maximumFileBytes,
                          std::size_t maximumFiles);

        void append(std::string_view line);

      private:
        [[nodiscard]] std::filesystem::path rotatedPath(std::size_t index) const;
        void rotate();

        std::filesystem::path filePath_;
        std::uintmax_t maximumFileBytes_;
        std::size_t maximumFiles_;
        std::mutex mutex_;
    };

} // namespace ssa::platform
