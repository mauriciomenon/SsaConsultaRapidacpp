#include "platform/ExclusiveFilePublisher.h"

#include <cerrno>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <stdio.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace ssa::platform {

    ports::ExclusiveFilePublishResult
    publishFileExclusively(const std::filesystem::path& source,
                           const std::filesystem::path& destination) {
#ifdef _WIN32
        if (MoveFileW(source.c_str(), destination.c_str()) != 0) {
            return {ports::ExclusiveFilePublishStatus::Succeeded, {}};
        }
        const auto code = GetLastError();
        const std::error_code error(static_cast<int>(code), std::system_category());
        if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) {
            return {ports::ExclusiveFilePublishStatus::DestinationExists,
                    "MoveFileW failed code=" + std::to_string(code) +
                        " message=" + error.message()};
        }
        return {ports::ExclusiveFilePublishStatus::Failed,
                "MoveFileW failed code=" + std::to_string(code) + " message=" + error.message()};
#elif defined(__APPLE__)
        if (::renamex_np(source.c_str(), destination.c_str(), RENAME_EXCL) == 0) {
            return {ports::ExclusiveFilePublishStatus::Succeeded, {}};
        }
        const auto code = errno;
        const std::error_code error(code, std::generic_category());
        if (code == EEXIST) {
            return {ports::ExclusiveFilePublishStatus::DestinationExists,
                    "renamex_np failed code=" + std::to_string(code) +
                        " message=" + error.message()};
        }
        return {ports::ExclusiveFilePublishStatus::Failed,
                "renamex_np failed code=" + std::to_string(code) + " message=" + error.message()};
#elif defined(__linux__)
#ifdef SYS_renameat2
        if (::syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(),
                      RENAME_NOREPLACE) == 0) {
            return {ports::ExclusiveFilePublishStatus::Succeeded, {}};
        }
        const auto code = errno;
        const std::error_code error(code, std::generic_category());
        if (code == EEXIST) {
            return {ports::ExclusiveFilePublishStatus::DestinationExists,
                    "renameat2 failed code=" + std::to_string(code) +
                        " message=" + error.message()};
        }
        return {ports::ExclusiveFilePublishStatus::Failed,
                "renameat2 failed code=" + std::to_string(code) + " message=" + error.message()};
#else
        return {ports::ExclusiveFilePublishStatus::Failed,
                "renameat2 is unavailable at build time"};
#endif
#else
        return {ports::ExclusiveFilePublishStatus::Failed,
                "exclusive file publish is unsupported on this platform"};
#endif
    }

} // namespace ssa::platform
