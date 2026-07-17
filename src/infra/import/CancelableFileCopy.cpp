#include "infra/import/CancelableFileCopy.h"

#include "qt/FilesystemPath.h"

#include <QTemporaryFile>

#include <cstdint>
#include <fstream>
#include <optional>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#endif

namespace ssa::infra::importing {
    namespace {

        constexpr std::size_t kCopyBlockBytes = std::size_t{1024} * 1024;

        struct SourceSnapshot {
            std::uintmax_t size = 0;
            std::filesystem::file_time_type modified;
            std::string identity;

            [[nodiscard]] bool operator==(const SourceSnapshot& other) const {
                return size == other.size && modified == other.modified &&
                       identity == other.identity;
            }
        };

        std::optional<SourceSnapshot> snapshotSource(const std::filesystem::path& source,
                                                     std::error_code& error) {
            const auto modified = std::filesystem::last_write_time(source, error);
            if (error) {
                return std::nullopt;
            }
            const auto identity = inspectFileIdentity(source, error);
            if (!identity) {
                return std::nullopt;
            }
            return SourceSnapshot{identity->size, modified, identity->value};
        }

        bool replaceAtomically(const std::filesystem::path& source,
                               const std::filesystem::path& destination, std::error_code& error) {
#ifdef _WIN32
            if (MoveFileExW(source.c_str(), destination.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
                error.clear();
                return true;
            }
            error = {static_cast<int>(GetLastError()), std::system_category()};
            return false;
#else
            std::filesystem::rename(source, destination, error);
            return !error;
#endif
        }

    } // namespace

    std::optional<FileIdentitySnapshot> inspectFileIdentity(const std::filesystem::path& path,
                                                            std::error_code& error) {
#ifdef _WIN32
        const auto handle =
            CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            error = {static_cast<int>(GetLastError()), std::system_category()};
            return std::nullopt;
        }
        BY_HANDLE_FILE_INFORMATION information{};
        const bool inspected = GetFileInformationByHandle(handle, &information) != 0;
        const auto inspectError = inspected ? ERROR_SUCCESS : GetLastError();
        const bool closed = CloseHandle(handle) != 0;
        if (!inspected) {
            error = {static_cast<int>(inspectError), std::system_category()};
            return std::nullopt;
        }
        if (!closed) {
            error = {static_cast<int>(GetLastError()), std::system_category()};
            return std::nullopt;
        }
        if ((information.dwFileAttributes &
             (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
            error = std::make_error_code(std::errc::operation_not_permitted);
            return std::nullopt;
        }
        const auto fileIndex = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
                               information.nFileIndexLow;
        const auto size = (static_cast<std::uint64_t>(information.nFileSizeHigh) << 32) |
                          information.nFileSizeLow;
        error.clear();
        return FileIdentitySnapshot{std::to_string(information.dwVolumeSerialNumber) + ':' +
                                        std::to_string(fileIndex),
                                    size};
#else
        struct stat information{};
        if (::lstat(path.c_str(), &information) != 0) {
            error = {errno, std::generic_category()};
            return std::nullopt;
        }
        if (!S_ISREG(information.st_mode)) {
            error = std::make_error_code(std::errc::operation_not_permitted);
            return std::nullopt;
        }
        error.clear();
        return FileIdentitySnapshot{
            std::to_string(static_cast<std::uintmax_t>(information.st_dev)) + ':' +
                std::to_string(static_cast<std::uintmax_t>(information.st_ino)),
            static_cast<std::uintmax_t>(information.st_size)};
#endif
    }

    FileCopyResult copyFileAtomically(const FileCopyRequest& request,
                                      const std::stop_token stopToken) {
        const auto canceled = [&stopToken] { return stopToken.stop_requested(); };
        if (canceled()) {
            return {FileCopyStatus::Canceled, {}};
        }
        std::error_code error;
        const auto sourceSnapshot = snapshotSource(request.source, error);
        if (!sourceSnapshot) {
            return {FileCopyStatus::Failed,
                    "cannot inspect staged file source: " + error.message()};
        }
        error.clear();
        std::filesystem::create_directories(request.destination.parent_path(), error);
        if (error) {
            return {FileCopyStatus::Failed, "cannot create copy destination: " + error.message()};
        }

        auto temporaryTemplate = request.destination;
        temporaryTemplate += ".XXXXXX.part";
        QTemporaryFile output(qt::toQString(temporaryTemplate));
        const auto removeTemporary = [&output]() -> std::string {
            if (output.fileName().isEmpty() || !output.exists() || output.remove()) {
                return {};
            }
            return "staging cleanup failed operation=remove_copy_temporary path=" +
                   qt::toUtf8(qt::toFileSystemPath(output.fileName())) +
                   " error=" + output.errorString().toStdString() + " pending=true";
        };
        std::ifstream input(request.source, std::ios::binary);
        if (!input || !output.open()) {
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, "cannot open staged file copy"};
        }

        std::vector<char> buffer(kCopyBlockBytes);
        while (input) {
            if (canceled()) {
                output.close();
                const auto cleanupDiagnostic = removeTemporary();
                if (!cleanupDiagnostic.empty()) {
                    return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
                }
                return {FileCopyStatus::Canceled, {}};
            }
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto bytes = input.gcount();
            if (bytes > 0 && output.write(buffer.data(), bytes) != bytes) {
                output.close();
                const auto cleanupDiagnostic = removeTemporary();
                if (!cleanupDiagnostic.empty()) {
                    return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
                }
                return {FileCopyStatus::Failed, "cannot write staged file copy"};
            }
        }
        if (!input.eof()) {
            output.close();
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, "cannot read staged file source"};
        }
        const auto flushed = output.flush();
        output.close();
        if (!flushed) {
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, "cannot flush staged file copy"};
        }
        if (canceled()) {
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
            }
            return {FileCopyStatus::Canceled, {}};
        }
        const auto currentSourceSnapshot = snapshotSource(request.source, error);
        if (!currentSourceSnapshot) {
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, "cannot verify staged file source: " + error.message()};
        }
        if (*currentSourceSnapshot != *sourceSnapshot) {
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed,
                        "source changed during staged file copy; " + cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, "source changed during staged file copy"};
        }
        const auto temporary = qt::toFileSystemPath(output.fileName());
        if (!replaceAtomically(temporary, request.destination, error)) {
            const auto diagnostic = "cannot publish staged file copy: " + error.message();
            const auto cleanupDiagnostic = removeTemporary();
            if (!cleanupDiagnostic.empty()) {
                return {FileCopyStatus::CleanupFailed, diagnostic + "; " + cleanupDiagnostic};
            }
            return {FileCopyStatus::Failed, diagnostic};
        }
        output.setAutoRemove(false);
        return {FileCopyStatus::Succeeded, {}};
    }

} // namespace ssa::infra::importing
