#include "infra/import/ImportFileConsolidator.h"

#include "infra/import/CancelableFileCopy.h"
#include "infra/import/ImportPathValidation.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <set>
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
            std::array<std::byte, std::size_t{64} * 1024> buffer{};
            auto infoClass = FileIdBothDirectoryRestartInfo;
            const auto& nativeFilename = filename.native();
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
            const auto& destinationName = destination.native();
            const auto nameBytes = static_cast<DWORD>(destinationName.size() * sizeof(wchar_t));
            std::vector<std::byte> buffer(offsetof(FILE_RENAME_INFO, FileName) + nameBytes +
                                          sizeof(wchar_t));
            auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
            rename->ReplaceIfExists = FALSE;
            rename->RootDirectory = nullptr;
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

        void markAllFailed(ImportConsolidationResult& result, const ImportConsolidationPlan& plan,
                           const std::size_t moveCount) {
            result.failed = moveCount;
            for (std::size_t index = 0; index < plan.entries.size(); ++index) {
                result.entries[index].failed = plan.entries[index].moves.size();
            }
        }

    } // namespace

    ImportFileConsolidator::ImportFileConsolidator(std::filesystem::path inputFolder)
        : inputFolder_(std::move(inputFolder)) {}

    ImportConsolidationPlan
    ImportFileConsolidator::plan(const std::vector<ImportManifestEntry>& manifest,
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
                if (!isSafeImportFilename(destinationFilename)) {
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
    ImportFileConsolidator::consolidate(const std::vector<ImportManifestEntry>& manifest,
                                        const std::stop_token& stopToken) const {
        return consolidate(plan(manifest, stopToken), stopToken);
    }

    ImportConsolidationResult
    ImportFileConsolidator::consolidate(const ImportConsolidationPlan& plan,
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
            markAllFailed(result, plan, moveCount);
            result.error = plan.error;
            return result;
        }
        std::string inputDiagnostic;
        const auto inputRejection = inputDirectoryRejectionReason(inputFolder_, inputDiagnostic);
        if (!inputRejection.empty()) {
            markAllFailed(result, plan, moveCount);
            result.error = inputRejection;
            appendDiagnostic(result.error, inputDiagnostic);
            return result;
        }
        std::error_code inputPathError;
        const auto inputDirectory = std::filesystem::weakly_canonical(inputFolder_, inputPathError);
        if (inputPathError) {
            markAllFailed(result, plan, moveCount);
            result.error = "cannot resolve consolidation input root: " + inputPathError.message();
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
                    markAllFailed(result, plan, moveCount);
                    result.error = "cannot resolve consolidation journal paths: " +
                                   (sourcePathError ? sourcePathError.message()
                                                    : destinationPathError.message());
                    return result;
                }
                if (sourceParent != inputDirectory || destinationParent != expectedDestination) {
                    markAllFailed(result, plan, moveCount);
                    result.error = "consolidation journal contains a path outside the input root";
                    return result;
                }
            }
        }
        if (moveCount == 0) {
            return result;
        }
        if (!prepareDirectory(processedDirectory, result.error) ||
            !prepareDirectory(noSurvivorDirectory, result.error)) {
            markAllFailed(result, plan, moveCount);
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
            markAllFailed(result, plan, moveCount);
            result.error = "cannot anchor consolidation directories: " + handleError.message();
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

} // namespace ssa::infra::importing
