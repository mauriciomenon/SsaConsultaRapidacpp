#include "platform/RotatingLogWriter.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace ssa::platform {

    namespace {
        constexpr std::string_view kTruncatedSuffix = "... [truncated]";
    }

    RotatingLogWriter::RotatingLogWriter(std::filesystem::path filePath,
                                         const std::uintmax_t maximumFileBytes,
                                         const std::size_t maximumFiles)
        : filePath_(std::move(filePath)), maximumFileBytes_(maximumFileBytes),
          maximumFiles_(maximumFiles) {
        if (filePath_.empty() || maximumFileBytes_ <= kTruncatedSuffix.size() + 1 ||
            maximumFiles_ == 0) {
            throw std::invalid_argument("rotating log configuration must be positive");
        }
        std::filesystem::create_directories(filePath_.parent_path());
    }

    void RotatingLogWriter::append(const std::string_view line) {
        const std::scoped_lock lock(mutex_);
        std::string payload{line};
        const auto maximumContentBytes = static_cast<std::size_t>(maximumFileBytes_ - 1);
        if (payload.size() > maximumContentBytes) {
            payload.resize(maximumContentBytes - kTruncatedSuffix.size());
            payload += kTruncatedSuffix;
        }
        const auto payloadBytes = payload.size() + 1;
        std::error_code sizeError;
        const auto currentBytes = std::filesystem::file_size(filePath_, sizeError);
        if (!sizeError && currentBytes > 0 && currentBytes + payloadBytes > maximumFileBytes_) {
            rotate();
        }

        std::ofstream output(filePath_, std::ios::binary | std::ios::app);
        if (!output) {
            throw std::runtime_error("cannot open rotating log file");
        }
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.put('\n');
        if (!output) {
            throw std::runtime_error("cannot write rotating log file");
        }
    }

    std::filesystem::path RotatingLogWriter::rotatedPath(const std::size_t index) const {
        return std::filesystem::path{filePath_.string() + "." + std::to_string(index)};
    }

    void RotatingLogWriter::rotate() {
        if (maximumFiles_ == 1) {
            std::filesystem::remove(filePath_);
            return;
        }
        std::filesystem::remove(rotatedPath(maximumFiles_ - 1));
        for (std::size_t index = maximumFiles_ - 1; index > 1; --index) {
            const auto previous = rotatedPath(index - 1);
            if (std::filesystem::exists(previous)) {
                std::filesystem::rename(previous, rotatedPath(index));
            }
        }
        if (std::filesystem::exists(filePath_)) {
            std::filesystem::rename(filePath_, rotatedPath(1));
        }
    }

} // namespace ssa::platform
