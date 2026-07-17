#include "infra/import/ImportFileStager.h"

#include "infra/import/CancelableFileCopy.h"
#include "qt/FilesystemPath.h"

#include <QDateTime>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#endif

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxSelectedImportFiles = 64;
        constexpr std::uintmax_t kMaxImportFileBytes = 128ULL * 1024ULL * 1024ULL;
        constexpr std::uintmax_t kMaxImportBatchBytes = 1024ULL * 1024ULL * 1024ULL;
        constexpr std::size_t kMaxDestinationAttempts = 10'000;
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

        std::string lowercaseExtension(const std::filesystem::path& path) {
            auto extension = qt::toUtf8(path.extension());
            std::ranges::transform(extension, extension.begin(), [](const char ch) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            });
            return extension;
        }

        bool isXlsxFile(const std::filesystem::path& path) {
            return lowercaseExtension(path) == ".xlsx";
        }

        bool isLegacyXlsFile(const std::filesystem::path& path) {
            return lowercaseExtension(path) == ".xls";
        }

        bool isExcelLockFile(const std::filesystem::path& path) {
            return qt::toQString(path.filename()).startsWith(QStringLiteral("~$"));
        }

        bool isOwnedStagingArtifact(const std::filesystem::path& path) {
            return isXlsxFile(path) &&
                   qt::toQString(path.filename()).startsWith(QStringLiteral(".ssa-staged-"));
        }

        bool isSafeFilename(const std::filesystem::path& filename) {
            return !filename.empty() && filename == filename.filename() && filename != "." &&
                   filename != "..";
        }

        void recordDiscoveredFile(ImportStagingResult& result, const std::filesystem::path& path) {
            if (isExcelLockFile(path)) {
                ++result.unsupported;
                return;
            }
            if (isLegacyXlsFile(path)) {
                ++result.legacyXls;
                return;
            }
            if (!isXlsxFile(path)) {
                ++result.unsupported;
                return;
            }
            result.discoveredXlsxSources.push_back(qt::toUtf8(path.filename()));
        }

        std::string sourceModifiedTimestamp(const std::filesystem::path& source,
                                            std::error_code& error) {
            const auto modified = std::filesystem::last_write_time(source, error);
            if (error) {
                return {};
            }
            const auto instant = std::chrono::floor<std::chrono::system_clock::duration>(
                std::filesystem::file_time_type::clock::to_sys(modified));
            const auto time = std::chrono::system_clock::to_time_t(instant);
            std::tm local{};
#ifdef _WIN32
            const bool conversionFailed = localtime_s(&local, &time) != 0;
#else
            const bool conversionFailed = localtime_r(&time, &local) == nullptr;
#endif
            if (conversionFailed) {
                error = std::make_error_code(std::errc::invalid_argument);
                return {};
            }
            std::array<char, 20> buffer{};
            if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &local) == 0) {
                error = std::make_error_code(std::errc::value_too_large);
                return {};
            }
            return buffer.data();
        }

        std::string sourceCreatedTimestamp(const std::filesystem::path& source) {
            const QFileInfo information{qt::toQString(source)};
            const auto created = information.birthTime();
            if (!created.isValid()) {
                return {};
            }
            const auto encoded =
                created.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toUtf8();
            return {encoded.constData(), static_cast<std::size_t>(encoded.size())};
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

        std::string batchPrefix() {
            static std::atomic_uint64_t sequence{0};
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            return std::to_string(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()) +
                   "_" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        }

        std::string preflightFiles(const std::vector<std::filesystem::path>& files,
                                   const std::stop_token& stopToken, std::string& diagnostic,
                                   const bool enforceSelectionLimit) {
            if (stopToken.stop_requested()) {
                return "canceled";
            }
            if (enforceSelectionLimit && files.size() > kMaxSelectedImportFiles) {
                return "too_many_files max=64";
            }
            std::uintmax_t totalBytes = 0;
            for (const auto& file : files) {
                if (stopToken.stop_requested()) {
                    return "canceled";
                }
                std::error_code error;
                const auto status = std::filesystem::symlink_status(file, error);
                if (error) {
                    if (error == std::errc::no_such_file_or_directory) {
                        appendDiagnostic(diagnostic,
                                         "cannot read import file size: " + error.message());
                        return "file_size_unavailable";
                    }
                    appendDiagnostic(diagnostic,
                                     "cannot inspect import source: " + error.message());
                    return "source_status_unavailable";
                }
                if (std::filesystem::is_symlink(status)) {
                    return "source_symlink";
                }
                if (!std::filesystem::is_regular_file(status)) {
                    return "source_not_regular";
                }
                const auto fileBytes = std::filesystem::file_size(file, error);
                if (error) {
                    appendDiagnostic(diagnostic,
                                     "cannot read import file size: " + error.message());
                    return "file_size_unavailable";
                }
                if (fileBytes > kMaxImportFileBytes) {
                    return "file_too_large max_bytes=134217728";
                }
                if (totalBytes > kMaxImportBatchBytes - fileBytes) {
                    return "batch_too_large max_bytes=1073741824";
                }
                totalBytes += fileBytes;
            }
            return {};
        }

        enum class DirectoryEntryState {
            Missing,
            RegularFile,
            Invalid,
            Error,
        };

#ifdef _WIN32
        class DirectoryHandle final {
          public:
            DirectoryHandle(const std::filesystem::path& path, std::error_code& error)
                : handle_(CreateFileW(path.c_str(), FILE_LIST_DIRECTORY | FILE_TRAVERSE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                      nullptr)) {
                if (handle_ == INVALID_HANDLE_VALUE) {
                    error = {static_cast<int>(GetLastError()), std::system_category()};
                    return;
                }
                FILE_ATTRIBUTE_TAG_INFO attributes{};
                if (GetFileInformationByHandleEx(handle_, FileAttributeTagInfo, &attributes,
                                                 sizeof(attributes)) == 0) {
                    const auto nativeError = GetLastError();
                    CloseHandle(handle_);
                    handle_ = INVALID_HANDLE_VALUE;
                    error = {static_cast<int>(nativeError), std::system_category()};
                    return;
                }
                if ((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                    (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    CloseHandle(handle_);
                    handle_ = INVALID_HANDLE_VALUE;
                    error = {static_cast<int>(ERROR_CANT_ACCESS_FILE), std::system_category()};
                    return;
                }
                error.clear();
            }

            ~DirectoryHandle() {
                if (handle_ != INVALID_HANDLE_VALUE) {
                    CloseHandle(handle_);
                }
            }

            DirectoryHandle(const DirectoryHandle&) = delete;
            DirectoryHandle& operator=(const DirectoryHandle&) = delete;

            [[nodiscard]] HANDLE native() const noexcept {
                return handle_;
            }

            [[nodiscard]] bool valid() const noexcept {
                return handle_ != INVALID_HANDLE_VALUE;
            }

          private:
            HANDLE handle_ = INVALID_HANDLE_VALUE;
        };

        std::wstring finalHandlePath(const HANDLE handle, std::error_code& error) {
            const DWORD required = GetFinalPathNameByHandleW(
                handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (required == 0) {
                error = {static_cast<int>(GetLastError()), std::system_category()};
                return {};
            }
            std::wstring path(required, L'\0');
            const DWORD written = GetFinalPathNameByHandleW(handle, path.data(), required,
                                                            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (written == 0 || written >= required) {
                error = {static_cast<int>(GetLastError()), std::system_category()};
                return {};
            }
            path.resize(written);
            error.clear();
            return path;
        }

        bool directoryIsDirectChild(const DirectoryHandle& parent, const DirectoryHandle& child,
                                    const std::filesystem::path&, std::error_code& error) {
            std::error_code parentError;
            const auto parentPath = finalHandlePath(parent.native(), parentError);
            std::error_code childError;
            const auto childPath = finalHandlePath(child.native(), childError);
            if (parentError || childError) {
                error = parentError ? parentError : childError;
                return false;
            }
            if (CompareStringOrdinal(std::filesystem::path{childPath}.parent_path().c_str(), -1,
                                     std::filesystem::path{parentPath}.c_str(), -1,
                                     TRUE) != CSTR_EQUAL) {
                error = std::make_error_code(std::errc::permission_denied);
                return false;
            }
            error.clear();
            return true;
        }

        DirectoryEntryState directoryEntryState(const DirectoryHandle& directory,
                                                const std::filesystem::path& filename,
                                                std::error_code& error) {
            std::array<std::byte, 64 * 1024> buffer{};
            auto infoClass = FileIdBothDirectoryRestartInfo;
            const auto nativeFilename = filename.native();
            for (;;) {
                if (GetFileInformationByHandleEx(directory.native(), infoClass, buffer.data(),
                                                 static_cast<DWORD>(buffer.size())) == 0) {
                    const auto nativeError = GetLastError();
                    if (nativeError == ERROR_NO_MORE_FILES) {
                        error.clear();
                        return DirectoryEntryState::Missing;
                    }
                    error = {static_cast<int>(nativeError), std::system_category()};
                    return DirectoryEntryState::Error;
                }
                auto* entry = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(buffer.data());
                for (;;) {
                    const int matches = CompareStringOrdinal(
                        entry->FileName, static_cast<int>(entry->FileNameLength / sizeof(wchar_t)),
                        nativeFilename.c_str(), static_cast<int>(nativeFilename.size()), TRUE);
                    if (matches == CSTR_EQUAL) {
                        error.clear();
                        const DWORD rejected =
                            FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
                        return (entry->FileAttributes & rejected) == 0
                                   ? DirectoryEntryState::RegularFile
                                   : DirectoryEntryState::Invalid;
                    }
                    if (entry->NextEntryOffset == 0) {
                        break;
                    }
                    entry = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(
                        reinterpret_cast<std::byte*>(entry) + entry->NextEntryOffset);
                }
                infoClass = FileIdBothDirectoryInfo;
            }
        }

        bool renameNoReplace(const DirectoryHandle& sourceDirectory,
                             const DirectoryHandle& destinationDirectory,
                             const std::filesystem::path& source,
                             const std::filesystem::path& destination, std::error_code& error) {
            HANDLE sourceHandle =
                CreateFileW(source.c_str(), DELETE | FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (sourceHandle == INVALID_HANDLE_VALUE) {
                error = {static_cast<int>(GetLastError()), std::system_category()};
                return false;
            }
            FILE_ATTRIBUTE_TAG_INFO attributes{};
            if (GetFileInformationByHandleEx(sourceHandle, FileAttributeTagInfo, &attributes,
                                             sizeof(attributes)) == 0) {
                error = {static_cast<int>(GetLastError()), std::system_category()};
                CloseHandle(sourceHandle);
                return false;
            }
            if ((attributes.FileAttributes &
                 (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
                error = std::make_error_code(std::errc::permission_denied);
                CloseHandle(sourceHandle);
                return false;
            }
            std::error_code parentError;
            const auto sourceParent = finalHandlePath(sourceDirectory.native(), parentError);
            std::error_code sourceError;
            const auto openedSource = finalHandlePath(sourceHandle, sourceError);
            if (parentError || sourceError ||
                CompareStringOrdinal(std::filesystem::path{openedSource}.parent_path().c_str(), -1,
                                     std::filesystem::path{sourceParent}.c_str(), -1,
                                     TRUE) != CSTR_EQUAL) {
                CloseHandle(sourceHandle);
                error = parentError   ? parentError
                        : sourceError ? sourceError
                                      : std::make_error_code(std::errc::permission_denied);
                return false;
            }
            const auto destinationName = destination.filename().native();
            const DWORD nameBytes = static_cast<DWORD>(destinationName.size() * sizeof(wchar_t));
            std::vector<std::byte> buffer(offsetof(FILE_RENAME_INFO, FileName) + nameBytes);
            auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
            rename->ReplaceIfExists = FALSE;
            rename->RootDirectory = destinationDirectory.native();
            rename->FileNameLength = nameBytes;
            std::memcpy(rename->FileName, destinationName.data(), nameBytes);
            const bool renamed = SetFileInformationByHandle(sourceHandle, FileRenameInfo, rename,
                                                            static_cast<DWORD>(buffer.size())) != 0;
            if (!renamed) {
                const auto nativeError = GetLastError();
                error =
                    nativeError == ERROR_FILE_EXISTS || nativeError == ERROR_ALREADY_EXISTS
                        ? std::make_error_code(std::errc::file_exists)
                        : std::error_code{static_cast<int>(nativeError), std::system_category()};
            } else {
                error.clear();
            }
            CloseHandle(sourceHandle);
            return renamed;
        }
#else
        class DirectoryHandle final {
          public:
            DirectoryHandle(const std::filesystem::path& path, std::error_code& error)
                : descriptor_(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)) {
                if (descriptor_ < 0) {
                    error = {errno, std::generic_category()};
                } else {
                    error.clear();
                }
            }

            ~DirectoryHandle() {
                if (descriptor_ >= 0) {
                    close(descriptor_);
                }
            }

            DirectoryHandle(const DirectoryHandle&) = delete;
            DirectoryHandle& operator=(const DirectoryHandle&) = delete;

            [[nodiscard]] int native() const noexcept {
                return descriptor_;
            }

            [[nodiscard]] bool valid() const noexcept {
                return descriptor_ >= 0;
            }

          private:
            int descriptor_ = -1;
        };

        bool directoryIsDirectChild(const DirectoryHandle& parent, const DirectoryHandle& child,
                                    const std::filesystem::path& filename, std::error_code& error) {
            struct stat childStatus{};
            if (fstat(child.native(), &childStatus) != 0) {
                error = {errno, std::generic_category()};
                return false;
            }
            struct stat anchoredStatus{};
            if (fstatat(parent.native(), filename.c_str(), &anchoredStatus, AT_SYMLINK_NOFOLLOW) !=
                0) {
                error = {errno, std::generic_category()};
                return false;
            }
            if (!S_ISDIR(anchoredStatus.st_mode) || childStatus.st_dev != anchoredStatus.st_dev ||
                childStatus.st_ino != anchoredStatus.st_ino) {
                error = std::make_error_code(std::errc::permission_denied);
                return false;
            }
            error.clear();
            return true;
        }

        DirectoryEntryState directoryEntryState(const DirectoryHandle& directory,
                                                const std::filesystem::path& filename,
                                                std::error_code& error) {
            struct stat status{};
            if (fstatat(directory.native(), filename.c_str(), &status, AT_SYMLINK_NOFOLLOW) == 0) {
                error.clear();
                return S_ISREG(status.st_mode) ? DirectoryEntryState::RegularFile
                                               : DirectoryEntryState::Invalid;
            }
            if (errno == ENOENT) {
                error.clear();
                return DirectoryEntryState::Missing;
            }
            error = {errno, std::generic_category()};
            return DirectoryEntryState::Error;
        }

        bool renameNoReplace(const DirectoryHandle& sourceDirectory,
                             const DirectoryHandle& destinationDirectory,
                             const std::filesystem::path& source,
                             const std::filesystem::path& destination, std::error_code& error) {
#if defined(__APPLE__)
            const int result = renameatx_np(sourceDirectory.native(), source.filename().c_str(),
                                            destinationDirectory.native(),
                                            destination.filename().c_str(), RENAME_EXCL);
#elif defined(__linux__)
            const int result = static_cast<int>(syscall(
                SYS_renameat2, sourceDirectory.native(), source.filename().c_str(),
                destinationDirectory.native(), destination.filename().c_str(), RENAME_NOREPLACE));
#else
#error "Atomic anchored no-replace rename is not implemented for this platform"
#endif
            if (result == 0) {
                error.clear();
                return true;
            }
            error = {errno, std::generic_category()};
            return false;
        }
#endif

        std::filesystem::path destinationCandidate(const std::filesystem::path& directory,
                                                   const std::filesystem::path& filename,
                                                   const std::size_t attempt) {
            if (attempt == 0) {
                return directory / filename;
            }
            auto candidate = filename.stem();
            candidate += "__" + std::to_string(attempt);
            candidate += filename.extension();
            return directory / candidate;
        }

        bool prepareDirectory(const std::filesystem::path& directory, std::string& message) {
            std::error_code error;
            const auto status = std::filesystem::symlink_status(directory, error);
            if (!error && std::filesystem::is_symlink(status)) {
                message = "consolidation destination is a symlink";
                return false;
            }
            error.clear();
            std::filesystem::create_directories(directory, error);
            if (error) {
                message = "cannot create consolidation directory: " + error.message();
                return false;
            }
            return true;
        }

        std::string cleanupOwnedArtifacts(const ImportStagingResult& result) {
            std::string diagnostic;
            std::error_code error;
            for (const auto& staged : result.files) {
                if (!staged.ownedByStager) {
                    continue;
                }
                std::filesystem::remove(staged.workbookPath, error);
                if (error) {
                    appendDiagnostic(diagnostic,
                                     "staging cleanup failed operation=remove_owned_staging path=" +
                                         qt::toUtf8(staged.workbookPath) +
                                         " error=" + error.message() + " pending=true");
                }
                error.clear();
            }
            return diagnostic;
        }

        void cancelStaging(ImportStagingResult& result) {
            const auto cleanupDiagnostic = cleanupOwnedArtifacts(result);
            appendDiagnostic(result.diagnostic, cleanupDiagnostic);
            result.files.clear();
            if (!cleanupDiagnostic.empty() || result.rejectionReason == "staging_cleanup_failed") {
                result.rejectionReason = "staging_cleanup_failed";
            } else {
                result.rejectionReason = "canceled";
            }
        }

    } // namespace

    ImportFileStager::ImportFileStager(std::filesystem::path inputFolder)
        : inputFolder_(std::move(inputFolder)) {}

    ImportStagingResult
    ImportFileStager::stageExternalFiles(const std::vector<std::filesystem::path>& files,
                                         const std::stop_token& stopToken) const {
        ImportStagingResult result;
        result.discovered = files.size();
        for (const auto& source : files) {
            recordDiscoveredFile(result, source);
        }
        result.rejectionReason = preflightFiles(files, stopToken, result.diagnostic, true);
        if (!result.rejectionReason.empty()) {
            result.operationalFailure = result.rejectionReason == "file_size_unavailable";
            return result;
        }
        result.rejectionReason = inputDirectoryRejectionReason(inputFolder_, result.diagnostic);
        if (!result.rejectionReason.empty()) {
            result.failedCopies = files.size();
            result.operationalFailure =
                result.rejectionReason == "input_directory_status_unavailable";
            return result;
        }
        std::error_code error;
        std::filesystem::create_directories(inputFolder_, error);
        if (error) {
            result.failedCopies = files.size();
            result.operationalFailure = true;
            result.rejectionReason = "input_directory_create_failed";
            appendDiagnostic(result.diagnostic,
                             "cannot create input directory: " + error.message());
            return result;
        }

        const auto prefix = batchPrefix();
        std::size_t fileIndex = 0;
        std::size_t summaryIndex = 0;
        for (const auto& source : files) {
            if (stopToken.stop_requested()) {
                cancelStaging(result);
                return result;
            }
            if (isExcelLockFile(source)) {
                continue;
            }
            const bool isXlsx = isXlsxFile(source);
            const auto currentSummaryIndex = summaryIndex;
            summaryIndex += isXlsx ? 1 : 0;
            const auto filename = source.filename();
            if (!isSafeFilename(filename)) {
                ++result.failedCopies;
                continue;
            }
            if (isLegacyXlsFile(source)) {
                continue;
            }
            if (!isXlsx) {
                continue;
            }
            const auto destination = stagedDestination({source, prefix, fileIndex});
            ++fileIndex;
            error.clear();
            const auto timestamp = sourceModifiedTimestamp(source, error);
            if (error) {
                ++result.failedCopies;
                appendDiagnostic(result.diagnostic,
                                 "cannot read source modification time: " + error.message());
                continue;
            }
            const auto copy = copyFileAtomically({source, destination}, stopToken);
            if (copy.status == FileCopyStatus::Canceled) {
                cancelStaging(result);
                return result;
            }
            if (!copy.ok()) {
                ++result.failedCopies;
                appendDiagnostic(result.diagnostic, copy.diagnostic);
                if (copy.status == FileCopyStatus::CleanupFailed) {
                    result.rejectionReason = "staging_cleanup_failed";
                    cancelStaging(result);
                    return result;
                }
                if (stopToken.stop_requested()) {
                    cancelStaging(result);
                    return result;
                }
                continue;
            }
            result.files.push_back({destination,
                                    {destination},
                                    true,
                                    qt::toUtf8(source.filename()),
                                    timestamp,
                                    currentSummaryIndex,
                                    sourceCreatedTimestamp(source),
                                    source.filename()});
        }
        if (stopToken.stop_requested()) {
            cancelStaging(result);
        }
        return result;
    }

    std::string ImportFileStager::discardOwnedArtifacts(const ImportStagingResult& staging) const {
        return cleanupOwnedArtifacts(staging);
    }

    ImportStagingResult
    ImportFileStager::validateInputDirectory(const std::stop_token& stopToken) const {
        ImportStagingResult result;
        if (stopToken.stop_requested()) {
            result.rejectionReason = "canceled";
            return result;
        }
        result.rejectionReason = inputDirectoryRejectionReason(inputFolder_, result.diagnostic);
        if (!result.rejectionReason.empty()) {
            result.failedCopies = 1;
            result.operationalFailure =
                result.rejectionReason == "input_directory_status_unavailable";
        }
        return result;
    }

    ImportStagingResult ImportFileStager::stageInputFiles(const std::stop_token& stopToken,
                                                          const bool includeProcessed) const {
        auto result = validateInputDirectory(stopToken);
        if (!result.rejectionReason.empty()) {
            return result;
        }
        std::error_code error;
        const bool inputExists = std::filesystem::exists(inputFolder_, error);
        if (error) {
            result.failedCopies = 1;
            result.operationalFailure = true;
            result.rejectionReason = "input_directory_status_unavailable";
            appendDiagnostic(result.diagnostic,
                             "cannot inspect input directory existence: " + error.message());
            return result;
        }
        if (!inputExists) {
            return result;
        }
        const bool inputIsDirectory = std::filesystem::is_directory(inputFolder_, error);
        if (error || !inputIsDirectory) {
            result.failedCopies = 1;
            result.operationalFailure = static_cast<bool>(error);
            if (error) {
                result.rejectionReason = "input_directory_status_unavailable";
                appendDiagnostic(result.diagnostic,
                                 "cannot inspect input directory type: " + error.message());
            } else {
                result.rejectionReason = "input_directory_not_directory";
            }
            return result;
        }
        std::filesystem::directory_iterator staleIterator(inputFolder_, error);
        if (error) {
            result.failedCopies = 1;
            result.operationalFailure = true;
            result.rejectionReason = "input_directory_unreadable";
            appendDiagnostic(result.diagnostic,
                             "cannot scan stale staging artifacts: " + error.message());
            return result;
        }
        for (auto iterator = staleIterator, end = std::filesystem::directory_iterator{};
             iterator != end;) {
            if (stopToken.stop_requested()) {
                result.rejectionReason = "canceled";
                return result;
            }
            const auto& entry = *iterator;
            if (!isOwnedStagingArtifact(entry.path())) {
                iterator.increment(error);
                if (error) {
                    result.failedCopies = 1;
                    result.operationalFailure = true;
                    result.rejectionReason = "staging_cleanup_scan_failed";
                    appendDiagnostic(result.diagnostic,
                                     "cannot continue scanning stale staging artifacts: " +
                                         error.message());
                    return result;
                }
                continue;
            }
            std::error_code removeError;
            if (!std::filesystem::remove(entry.path(), removeError) && removeError) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "staging_cleanup_failed";
                appendDiagnostic(result.diagnostic,
                                 "staging cleanup failed operation=remove_abandoned_staging path=" +
                                     qt::toUtf8(entry.path()) + " error=" + removeError.message() +
                                     " pending=true");
                return result;
            }
            iterator.increment(error);
            if (error) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "staging_cleanup_scan_failed";
                appendDiagnostic(result.diagnostic,
                                 "cannot continue scanning stale staging artifacts: " +
                                     error.message());
                return result;
            }
        }
        error.clear();
        std::vector<std::filesystem::path> candidates;
        std::filesystem::directory_iterator iterator(inputFolder_, error);
        if (error) {
            result.failedCopies = 1;
            result.operationalFailure = true;
            result.rejectionReason = "input_directory_unreadable";
            appendDiagnostic(result.diagnostic, "cannot scan input directory: " + error.message());
            return result;
        }
        for (auto current = iterator, end = std::filesystem::directory_iterator{};
             current != end;) {
            if (stopToken.stop_requested()) {
                result.rejectionReason = "canceled";
                return result;
            }
            const auto& entry = *current;
            const bool entryIsSymlink = entry.is_symlink(error);
            if (error) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "input_entry_status_unavailable";
                appendDiagnostic(result.diagnostic,
                                 "cannot inspect input entry symlink status: " + error.message());
                return result;
            }
            const bool entryIsRegular = entry.is_regular_file(error);
            if (error) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "input_entry_status_unavailable";
                appendDiagnostic(result.diagnostic,
                                 "cannot inspect input entry file status: " + error.message());
                return result;
            }
            if (entryIsSymlink) {
                ++result.discovered;
                ++result.unsupported;
            } else if (entryIsRegular) {
                ++result.discovered;
                candidates.push_back(entry.path());
                recordDiscoveredFile(result, entry.path());
            }
            current.increment(error);
            if (error) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "input_directory_scan_failed";
                appendDiagnostic(result.diagnostic,
                                 "cannot continue scanning input directory: " + error.message());
                return result;
            }
        }
        std::ranges::sort(candidates);

        std::vector<std::filesystem::path> processedWorkbooks;
        const auto processedDirectory = inputFolder_ / "processadas";
        const auto processedStatus = std::filesystem::symlink_status(processedDirectory, error);
        if (includeProcessed && error && error != std::errc::no_such_file_or_directory) {
            result.failedCopies = 1;
            result.operationalFailure = true;
            result.rejectionReason = "processed_directory_status_unavailable";
            appendDiagnostic(result.diagnostic,
                             "cannot inspect processed directory: " + error.message());
            return result;
        }
        error.clear();
        if (includeProcessed && std::filesystem::is_symlink(processedStatus)) {
            result.rejectionReason = "processed_directory_symlink";
            return result;
        } else if (includeProcessed && std::filesystem::exists(processedStatus)) {
            if (!std::filesystem::is_directory(processedStatus)) {
                result.failedCopies = 1;
                result.rejectionReason = "processed_directory_not_directory";
                return result;
            }
            std::filesystem::directory_iterator processedIterator(processedDirectory, error);
            if (error) {
                result.failedCopies = 1;
                result.operationalFailure = true;
                result.rejectionReason = "processed_directory_unreadable";
                appendDiagnostic(result.diagnostic,
                                 "cannot scan processed directory: " + error.message());
                return result;
            }
            for (auto current = processedIterator, end = std::filesystem::directory_iterator{};
                 current != end;) {
                if (stopToken.stop_requested()) {
                    result.rejectionReason = "canceled";
                    return result;
                }
                const auto& entry = *current;
                const bool entryIsSymlink = entry.is_symlink(error);
                if (error) {
                    result.failedCopies = 1;
                    result.operationalFailure = true;
                    result.rejectionReason = "processed_entry_status_unavailable";
                    appendDiagnostic(result.diagnostic,
                                     "cannot inspect processed entry: " + error.message());
                    return result;
                }
                const bool entryIsRegular = entry.is_regular_file(error);
                if (error) {
                    result.failedCopies = 1;
                    result.operationalFailure = true;
                    result.rejectionReason = "processed_entry_status_unavailable";
                    appendDiagnostic(result.diagnostic,
                                     "cannot inspect processed entry: " + error.message());
                    return result;
                }
                if (!entryIsSymlink && entryIsRegular && isXlsxFile(entry.path())) {
                    ++result.discovered;
                    processedWorkbooks.push_back(entry.path());
                    recordDiscoveredFile(result, entry.path());
                }
                current.increment(error);
                if (error) {
                    result.failedCopies = 1;
                    result.operationalFailure = true;
                    result.rejectionReason = "processed_directory_scan_failed";
                    appendDiagnostic(result.diagnostic,
                                     "cannot continue scanning processed directory: " +
                                         error.message());
                    return result;
                }
            }
            std::ranges::sort(processedWorkbooks);
        }

        std::vector<std::filesystem::path> importCandidates;
        std::ranges::copy_if(
            candidates, std::back_inserter(importCandidates),
            [](const auto& path) { return !isExcelLockFile(path) && isXlsxFile(path); });
        importCandidates.insert(importCandidates.end(), processedWorkbooks.begin(),
                                processedWorkbooks.end());
        result.discoveredXlsxSources.clear();
        result.discoveredXlsxSources.reserve(importCandidates.size());
        for (const auto& path : importCandidates) {
            result.discoveredXlsxSources.push_back(qt::toUtf8(path.filename()));
        }
        result.rejectionReason =
            preflightFiles(importCandidates, stopToken, result.diagnostic, false);
        if (!result.rejectionReason.empty()) {
            result.operationalFailure = result.rejectionReason == "file_size_unavailable";
            return result;
        }

        std::size_t summaryIndex = 0;
        for (const auto& path : candidates) {
            if (stopToken.stop_requested()) {
                cancelStaging(result);
                return result;
            }
            if (isExcelLockFile(path)) {
                continue;
            }
            if (isLegacyXlsFile(path)) {
                continue;
            }
            if (isXlsxFile(path)) {
                const auto currentSummaryIndex = summaryIndex++;
                error.clear();
                const auto timestamp = sourceModifiedTimestamp(path, error);
                if (error) {
                    ++result.failedCopies;
                    result.operationalFailure = true;
                    appendDiagnostic(result.diagnostic,
                                     "cannot read source modification time: " + error.message());
                    continue;
                }
                result.files.push_back({path,
                                        {path},
                                        false,
                                        qt::toUtf8(path.filename()),
                                        timestamp,
                                        currentSummaryIndex,
                                        sourceCreatedTimestamp(path),
                                        path.filename()});
                continue;
            }
        }
        for (const auto& path : processedWorkbooks) {
            error.clear();
            const auto timestamp = sourceModifiedTimestamp(path, error);
            if (error) {
                ++result.failedCopies;
                result.operationalFailure = true;
                appendDiagnostic(result.diagnostic,
                                 "cannot read source modification time: " + error.message());
                continue;
            }
            result.files.push_back({path, {}, false, qt::toUtf8(path.filename()), timestamp});
            result.files.back().sourceCreatedTimestamp = sourceCreatedTimestamp(path);
            result.files.back().summaryIndex = summaryIndex++;
            result.files.back().consolidationFilename = path.filename();
        }
        if (stopToken.stop_requested()) {
            cancelStaging(result);
            return result;
        }
        std::ranges::sort(result.files, {}, &StagedImportFile::workbookPath);
        return result;
    }

    ImportConsolidationPlan
    ImportFileStager::planConsolidation(const std::vector<ImportManifestEntry>& manifest,
                                        const std::stop_token& stopToken) const {
        ImportConsolidationPlan plan;
        plan.entries.resize(manifest.size());
        const auto inputDirectory = std::filesystem::absolute(inputFolder_).lexically_normal();
        const auto processedDirectory = inputDirectory / "processadas";
        const auto noSurvivorDirectory = processedDirectory / "nosurvivor";
        std::set<std::filesystem::path> reservedDestinations;
        for (std::size_t entryIndex = 0; entryIndex < manifest.size(); ++entryIndex) {
            const auto& manifestEntry = manifest[entryIndex];
            auto& planEntry = plan.entries[entryIndex];
            if (!manifestEntry.destinationFilename.empty() && manifestEntry.sources.size() > 1) {
                plan.error = "consolidation filename accepts at most one source";
                return plan;
            }
            const auto& destinationDirectory =
                manifestEntry.hasValidRows ? processedDirectory : noSurvivorDirectory;
            std::error_code directoryError;
            const auto directoryStatus =
                std::filesystem::symlink_status(destinationDirectory, directoryError);
            const bool inspectExisting = !directoryError &&
                                         std::filesystem::is_directory(directoryStatus) &&
                                         !std::filesystem::is_symlink(directoryStatus);
            for (const auto& source : manifestEntry.sources) {
                if (stopToken.stop_requested()) {
                    plan.canceled = true;
                    plan.error = "consolidation planning canceled";
                    return plan;
                }
                const auto normalizedSource = std::filesystem::absolute(source).lexically_normal();
                std::error_code identityError;
                const auto sourceSnapshot = inspectFileIdentity(normalizedSource, identityError);
                if (!sourceSnapshot) {
                    plan.error =
                        "cannot inspect consolidation source identity: " + identityError.message();
                    return plan;
                }
                const auto destinationFilename = manifestEntry.destinationFilename.empty()
                                                     ? normalizedSource.filename()
                                                     : manifestEntry.destinationFilename;
                if (!isSafeFilename(destinationFilename)) {
                    plan.error = "unsafe consolidation destination filename";
                    return plan;
                }
                bool destinationSelected = false;
                for (std::size_t attempt = 0; attempt < kMaxDestinationAttempts; ++attempt) {
                    auto candidate =
                        destinationCandidate(destinationDirectory, destinationFilename, attempt);
                    if (reservedDestinations.contains(candidate)) {
                        continue;
                    }
                    if (inspectExisting) {
                        std::error_code candidateError;
                        const auto candidateStatus =
                            std::filesystem::symlink_status(candidate, candidateError);
                        if (candidateError &&
                            candidateError != std::errc::no_such_file_or_directory) {
                            plan.error = "cannot inspect consolidation destination: " +
                                         candidateError.message();
                            return plan;
                        }
                        if (!candidateError && std::filesystem::exists(candidateStatus)) {
                            continue;
                        }
                    }
                    reservedDestinations.insert(candidate);
                    planEntry.moves.push_back({normalizedSource, std::move(candidate),
                                               manifestEntry.hasValidRows, sourceSnapshot->value,
                                               sourceSnapshot->size});
                    destinationSelected = true;
                    break;
                }
                if (!destinationSelected) {
                    plan.error = "cannot allocate unique consolidation destination";
                    return plan;
                }
            }
        }
        return plan;
    }

    ImportConsolidationResult
    ImportFileStager::consolidate(const std::vector<ImportManifestEntry>& manifest,
                                  const std::stop_token& stopToken) const {
        return consolidate(planConsolidation(manifest, stopToken), stopToken);
    }

    ImportConsolidationResult
    ImportFileStager::consolidate(const ImportConsolidationPlan& plan,
                                  const std::stop_token& stopToken) const {
        ImportConsolidationResult result;
        result.entries.resize(plan.entries.size());
        std::size_t moveCount = 0;
        for (std::size_t index = 0; index < plan.entries.size(); ++index) {
            const auto count = plan.entries[index].moves.size();
            moveCount += count;
            result.entries[index].moves.resize(count);
        }
        if (plan.canceled || stopToken.stop_requested()) {
            result.canceled = true;
            result.error = "consolidation canceled";
            return result;
        }
        if (!plan.error.empty()) {
            result.failed = moveCount;
            result.error = plan.error;
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
            return result;
        }
        std::string inputDiagnostic;
        const auto inputRejection = inputDirectoryRejectionReason(inputFolder_, inputDiagnostic);
        if (!inputRejection.empty()) {
            result.failed = moveCount;
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
            result.error = inputRejection;
            appendDiagnostic(result.error, inputDiagnostic);
            return result;
        }
        std::error_code inputPathError;
        const auto inputDirectory = std::filesystem::weakly_canonical(inputFolder_, inputPathError);
        if (inputPathError) {
            result.failed = moveCount;
            result.error = "cannot resolve consolidation input root: " + inputPathError.message();
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
            return result;
        }
        const auto processedDirectory = inputDirectory / "processadas";
        const auto noSurvivorDirectory = processedDirectory / "nosurvivor";
        for (const auto& entry : plan.entries) {
            for (const auto& move : entry.moves) {
                const auto expectedDestination =
                    move.hasValidRows ? processedDirectory : noSurvivorDirectory;
                std::error_code sourcePathError;
                const auto sourceParent =
                    std::filesystem::weakly_canonical(move.source.parent_path(), sourcePathError);
                std::error_code destinationPathError;
                const auto destinationParent = std::filesystem::weakly_canonical(
                    move.destination.parent_path(), destinationPathError);
                if (sourcePathError || destinationPathError) {
                    result.failed = moveCount;
                    result.error = "cannot resolve consolidation journal paths: " +
                                   (sourcePathError ? sourcePathError.message()
                                                    : destinationPathError.message());
                    for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                        result.entries[index].failed = plan.entries[index].moves.size();
                    }
                    return result;
                }
                if (sourceParent != inputDirectory || destinationParent != expectedDestination) {
                    result.failed = moveCount;
                    result.error = "consolidation journal contains a path outside the input root";
                    for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                        result.entries[index].failed = plan.entries[index].moves.size();
                    }
                    return result;
                }
            }
        }
        if (moveCount == 0) {
            return result;
        }
        if (!prepareDirectory(processedDirectory, result.error) ||
            !prepareDirectory(noSurvivorDirectory, result.error)) {
            result.failed = moveCount;
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
            return result;
        }
        std::error_code inputHandleError;
        DirectoryHandle inputHandle(inputDirectory, inputHandleError);
        std::error_code processedHandleError;
        DirectoryHandle processedHandle(processedDirectory, processedHandleError);
        std::error_code noSurvivorHandleError;
        DirectoryHandle noSurvivorHandle(noSurvivorDirectory, noSurvivorHandleError);
        std::error_code processedRelationError;
        const bool processedAnchored =
            inputHandle.valid() && processedHandle.valid() &&
            directoryIsDirectChild(inputHandle, processedHandle, processedDirectory.filename(),
                                   processedRelationError);
        std::error_code noSurvivorRelationError;
        const bool noSurvivorAnchored =
            processedHandle.valid() && noSurvivorHandle.valid() &&
            directoryIsDirectChild(processedHandle, noSurvivorHandle,
                                   noSurvivorDirectory.filename(), noSurvivorRelationError);
        if (!inputHandle.valid() || !processedHandle.valid() || !noSurvivorHandle.valid() ||
            !processedAnchored || !noSurvivorAnchored) {
            const auto handleError = inputHandleError         ? inputHandleError
                                     : processedHandleError   ? processedHandleError
                                     : noSurvivorHandleError  ? noSurvivorHandleError
                                     : processedRelationError ? processedRelationError
                                                              : noSurvivorRelationError;
            result.failed = moveCount;
            result.error = "cannot anchor consolidation directories: " + handleError.message();
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
            return result;
        }
        for (std::size_t entryIndex = 0; entryIndex < plan.entries.size(); ++entryIndex) {
            const auto& entry = plan.entries[entryIndex];
            auto& entryResult = result.entries[entryIndex];
            for (std::size_t moveIndex = 0; moveIndex < entry.moves.size(); ++moveIndex) {
                const auto& move = entry.moves[moveIndex];
                auto& moveResult = entryResult.moves[moveIndex];
                if (stopToken.stop_requested()) {
                    result.canceled = true;
                    result.error = "consolidation canceled";
                    return result;
                }
                const auto& destinationHandle =
                    move.hasValidRows ? processedHandle : noSurvivorHandle;
                std::error_code sourceError;
                const auto sourceState =
                    directoryEntryState(inputHandle, move.source.filename(), sourceError);
                if (sourceState == DirectoryEntryState::Error) {
                    result.error = "cannot inspect consolidation source: " + sourceError.message();
                    ++result.failed;
                    ++entryResult.failed;
                    moveResult.failed = true;
                    continue;
                }
                std::error_code destinationError;
                const auto destinationState = directoryEntryState(
                    destinationHandle, move.destination.filename(), destinationError);
                if (destinationState == DirectoryEntryState::Error) {
                    result.error =
                        "cannot inspect consolidation destination: " + destinationError.message();
                    ++result.failed;
                    ++entryResult.failed;
                    moveResult.failed = true;
                    continue;
                }
                if (sourceState == DirectoryEntryState::Missing &&
                    destinationState == DirectoryEntryState::RegularFile) {
                    if (move.sourceIdentity.empty() || !move.sourceSize) {
                        result.error =
                            "consolidation destination identity is unavailable for resume";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    std::error_code identityError;
                    const auto destinationSnapshot =
                        inspectFileIdentity(move.destination, identityError);
                    if (!destinationSnapshot) {
                        result.error = "cannot inspect consolidation destination identity: " +
                                       identityError.message();
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    if (destinationSnapshot->value != move.sourceIdentity) {
                        result.error = "consolidation destination identity does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    if (destinationSnapshot->size != *move.sourceSize) {
                        result.error = "consolidation destination size does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    moveResult.completed = true;
                    ++entryResult.completed;
                    if (!move.hasValidRows) {
                        ++result.noSurvivor;
                        ++entryResult.noSurvivor;
                    }
                    continue;
                }
                if (sourceState != DirectoryEntryState::RegularFile ||
                    destinationState != DirectoryEntryState::Missing) {
                    result.error = "consolidation source and destination state is ambiguous";
                    ++result.failed;
                    ++entryResult.failed;
                    moveResult.failed = true;
                    continue;
                }
                if (move.sourceIdentity.empty() != !move.sourceSize) {
                    result.error = "consolidation journal identity is incomplete";
                    ++result.failed;
                    ++entryResult.failed;
                    moveResult.failed = true;
                    continue;
                }
                if (!move.sourceIdentity.empty()) {
                    std::error_code identityError;
                    const auto sourceSnapshot = inspectFileIdentity(move.source, identityError);
                    if (!sourceSnapshot) {
                        result.error = "cannot inspect consolidation source identity: " +
                                       identityError.message();
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    if (sourceSnapshot->value != move.sourceIdentity) {
                        result.error = "consolidation source identity does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    if (sourceSnapshot->size != *move.sourceSize) {
                        result.error = "consolidation source size does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                }
                std::error_code renameError;
                if (!renameNoReplace(inputHandle, destinationHandle, move.source, move.destination,
                                     renameError)) {
                    result.error = "cannot move consolidated file: " + renameError.message();
                    ++result.failed;
                    ++entryResult.failed;
                    moveResult.failed = true;
                    continue;
                }
                if (!move.sourceIdentity.empty()) {
                    std::error_code identityError;
                    const auto destinationSnapshot =
                        inspectFileIdentity(move.destination, identityError);
                    if (!destinationSnapshot || destinationSnapshot->value != move.sourceIdentity) {
                        result.error = !destinationSnapshot
                                           ? "cannot verify moved consolidation identity: " +
                                                 identityError.message()
                                           : "moved consolidation identity does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                    if (destinationSnapshot->size != *move.sourceSize) {
                        result.error = "moved consolidation size does not match journal";
                        ++result.failed;
                        ++entryResult.failed;
                        moveResult.failed = true;
                        continue;
                    }
                }
                moveResult.completed = true;
                moveResult.moved = true;
                ++entryResult.completed;
                ++result.moved;
                ++entryResult.moved;
                if (!move.hasValidRows) {
                    ++result.noSurvivor;
                    ++entryResult.noSurvivor;
                }
            }
        }
        return result;
    }

    std::filesystem::path
    ImportFileStager::stagedDestination(const StagedDestinationRequest& request) const {
        std::filesystem::path candidateName = ".ssa-staged-";
        candidateName += request.source.stem();
        candidateName += "_";
        candidateName += request.batchPrefix;
        candidateName += "_";
        candidateName += std::to_string(request.fileIndex);
        candidateName += request.source.extension();
        return inputFolder_ / candidateName;
    }

} // namespace ssa::infra::importing
