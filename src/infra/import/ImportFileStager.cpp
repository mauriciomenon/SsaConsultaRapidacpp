#include "infra/import/ImportFileStager.h"

#include "infra/import/CancelableFileCopy.h"
#include "qt/FilesystemPath.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdio>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxImportFiles = 64;
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

        bool isSafeFilename(const std::filesystem::path& filename) {
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

        std::string batchPrefix() {
            static std::atomic_uint64_t sequence{0};
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            return std::to_string(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()) +
                   "_" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        }

        std::string preflightFiles(const std::vector<std::filesystem::path>& files,
                                   const std::stop_token& stopToken, std::string& diagnostic) {
            if (stopToken.stop_requested()) {
                return "canceled";
            }
            if (files.size() > kMaxImportFiles) {
                return "too_many_files max=64";
            }
            std::uintmax_t totalBytes = 0;
            for (const auto& file : files) {
                if (stopToken.stop_requested()) {
                    return "canceled";
                }
                std::error_code error;
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

        bool renameNoReplace(const std::filesystem::path& source,
                             const std::filesystem::path& destination, std::error_code& error) {
#ifdef _WIN32
            if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != 0) {
                error.clear();
                return true;
            }
            const auto nativeError = GetLastError();
            error = nativeError == ERROR_FILE_EXISTS || nativeError == ERROR_ALREADY_EXISTS
                        ? std::make_error_code(std::errc::file_exists)
                        : std::error_code{static_cast<int>(nativeError), std::system_category()};
#elif defined(__APPLE__)
            if (renamex_np(source.c_str(), destination.c_str(), RENAME_EXCL) == 0) {
                error.clear();
                return true;
            }
            error = std::error_code{errno, std::generic_category()};
#elif defined(__linux__)
            if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(),
                        RENAME_NOREPLACE) == 0) {
                error.clear();
                return true;
            }
            error = std::error_code{errno, std::generic_category()};
#else
#error "Atomic no-replace rename is not implemented for this platform"
#endif
            return false;
        }

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

        bool moveToUniqueDestination(const std::filesystem::path& source,
                                     const std::filesystem::path& directory,
                                     std::filesystem::path& destination, std::string& message,
                                     const std::stop_token& stopToken) {
            for (std::size_t attempt = 0; attempt < kMaxDestinationAttempts; ++attempt) {
                if (stopToken.stop_requested()) {
                    message = "consolidation canceled";
                    return false;
                }
                auto candidate = destinationCandidate(directory, source.filename(), attempt);
                std::error_code error;
                if (renameNoReplace(source, candidate, error)) {
                    destination = std::move(candidate);
                    return true;
                }
                if (error == std::errc::file_exists) {
                    continue;
                }
                message = "cannot move consolidated file: " + error.message();
                return false;
            }
            message = "cannot allocate unique consolidation destination";
            return false;
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
                                     "cannot remove canceled staged file: " + error.message());
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

        std::size_t manifestSourceCount(const std::vector<ImportManifestEntry>& manifest) {
            std::size_t count = 0;
            for (const auto& entry : manifest) {
                count += entry.sources.size();
            }
            return count;
        }

    } // namespace

    ImportFileStager::ImportFileStager(std::filesystem::path inputFolder,
                                       LegacySpreadsheetConverter legacyConverter)
        : inputFolder_(std::move(inputFolder)), legacyConverter_(std::move(legacyConverter)) {}

    ImportStagingResult
    ImportFileStager::stageExternalFiles(const std::vector<std::filesystem::path>& files,
                                         const std::stop_token& stopToken) const {
        ImportStagingResult result;
        result.rejectionReason = preflightFiles(files, stopToken, result.diagnostic);
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
        for (const auto& source : files) {
            if (stopToken.stop_requested()) {
                cancelStaging(result);
                return result;
            }
            if (isExcelLockFile(source)) {
                ++result.unsupported;
                continue;
            }
            const auto filename = source.filename();
            if (!isSafeFilename(filename)) {
                ++result.failedCopies;
                continue;
            }
            if (isLegacyXlsFile(source)) {
                const auto destination = stagedDestination({source, prefix, fileIndex});
                ++fileIndex;
                auto xlsxDestination = destination;
                xlsxDestination.replace_extension(".xlsx");
                stageLegacyFile({source, xlsxDestination}, {xlsxDestination}, result, stopToken);
                if (stopToken.stop_requested()) {
                    cancelStaging(result);
                    return result;
                }
                continue;
            }
            if (!isXlsxFile(source)) {
                ++result.unsupported;
                continue;
            }
            const auto destination = stagedDestination({source, prefix, fileIndex});
            ++fileIndex;
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
            result.files.push_back({destination, {destination}, true});
        }
        if (stopToken.stop_requested()) {
            cancelStaging(result);
        }
        return result;
    }

    std::string ImportFileStager::discardOwnedArtifacts(const ImportStagingResult& staging) const {
        return cleanupOwnedArtifacts(staging);
    }

    ImportStagingResult ImportFileStager::stageInputFiles(const std::stop_token& stopToken,
                                                          const bool includeProcessed) const {
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
        std::vector<std::filesystem::path> candidates;
        std::filesystem::directory_iterator iterator(inputFolder_, error);
        if (error) {
            result.failedCopies = 1;
            result.operationalFailure = true;
            result.rejectionReason = "input_directory_unreadable";
            appendDiagnostic(result.diagnostic, "cannot scan input directory: " + error.message());
            return result;
        }
        for (const auto& entry : iterator) {
            if (stopToken.stop_requested()) {
                result.rejectionReason = "canceled";
                return result;
            }
            if (entry.is_symlink(error)) {
                ++result.unsupported;
                error.clear();
                continue;
            }
            if (!entry.is_regular_file(error) || error) {
                error.clear();
                continue;
            }
            candidates.push_back(entry.path());
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
            ++result.unsupported;
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
            for (const auto& entry : processedIterator) {
                if (stopToken.stop_requested()) {
                    result.rejectionReason = "canceled";
                    return result;
                }
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
                    processedWorkbooks.push_back(entry.path());
                }
            }
            std::ranges::sort(processedWorkbooks);
        }

        std::vector<std::filesystem::path> importCandidates;
        std::ranges::copy_if(
            candidates, std::back_inserter(importCandidates), [](const auto& path) {
                return !isExcelLockFile(path) && (isLegacyXlsFile(path) || isXlsxFile(path));
            });
        importCandidates.insert(importCandidates.end(), processedWorkbooks.begin(),
                                processedWorkbooks.end());
        result.rejectionReason = preflightFiles(importCandidates, stopToken, result.diagnostic);
        if (!result.rejectionReason.empty()) {
            result.operationalFailure = result.rejectionReason == "file_size_unavailable";
            return result;
        }

        for (const auto& path : candidates) {
            if (stopToken.stop_requested()) {
                cancelStaging(result);
                return result;
            }
            if (isExcelLockFile(path)) {
                ++result.unsupported;
                continue;
            }
            if (isLegacyXlsFile(path)) {
                auto destination = path;
                destination.replace_extension(".xlsx");
                if (std::filesystem::exists(destination, error)) {
                    ++result.legacyXls;
                    continue;
                }
                error.clear();
                stageLegacyFile({path, destination}, {path, destination}, result, stopToken);
                if (stopToken.stop_requested() || result.rejectionReason == "canceled") {
                    cancelStaging(result);
                    return result;
                }
                continue;
            }
            if (isXlsxFile(path)) {
                result.files.push_back({path, {path}});
                continue;
            }
            ++result.unsupported;
        }
        for (const auto& path : processedWorkbooks) {
            result.files.push_back({path, {}});
        }
        if (stopToken.stop_requested()) {
            cancelStaging(result);
            return result;
        }
        std::ranges::sort(result.files, {}, &StagedImportFile::workbookPath);
        return result;
    }

    ImportConsolidationResult
    ImportFileStager::consolidate(const std::vector<ImportManifestEntry>& manifest,
                                  const std::stop_token& stopToken) const {
        ImportConsolidationResult result;
        if (stopToken.stop_requested()) {
            result.canceled = true;
            result.error = "consolidation canceled";
            return result;
        }
        std::string inputDiagnostic;
        const auto inputRejection = inputDirectoryRejectionReason(inputFolder_, inputDiagnostic);
        if (!inputRejection.empty()) {
            result.failed = manifestSourceCount(manifest);
            result.error = inputRejection;
            appendDiagnostic(result.error, inputDiagnostic);
            return result;
        }
        if (manifest.empty()) {
            return result;
        }
        const auto processedDirectory = inputFolder_ / "processadas";
        const auto noSurvivorDirectory = processedDirectory / "nosurvivor";
        if (!prepareDirectory(processedDirectory, result.error) ||
            !prepareDirectory(noSurvivorDirectory, result.error)) {
            result.failed = manifestSourceCount(manifest);
            return result;
        }
        for (const auto& entry : manifest) {
            if (stopToken.stop_requested()) {
                result.canceled = true;
                result.error = "consolidation canceled";
                return result;
            }
            const auto& destinationDirectory =
                entry.hasValidRows ? processedDirectory : noSurvivorDirectory;
            for (const auto& source : entry.sources) {
                if (stopToken.stop_requested()) {
                    result.canceled = true;
                    result.error = "consolidation canceled";
                    return result;
                }
                std::filesystem::path destination;
                if (!moveToUniqueDestination(source, destinationDirectory, destination,
                                             result.error, stopToken)) {
                    if (stopToken.stop_requested()) {
                        result.canceled = true;
                        return result;
                    }
                    ++result.failed;
                    continue;
                }
                ++result.moved;
                if (!entry.hasValidRows) {
                    ++result.noSurvivor;
                }
            }
        }
        return result;
    }

    bool ImportFileStager::stageLegacyFile(const LegacyStageRequest& request,
                                           std::vector<std::filesystem::path> consolidationSources,
                                           ImportStagingResult& result,
                                           const std::stop_token& stopToken) const {
        ++result.legacyXls;
        const auto conversion =
            legacyConverter_.convertToXlsx({request.source, request.destination}, stopToken);
        if (!conversion.ok()) {
            if (conversion.status == LegacySpreadsheetConversionStatus::Canceled) {
                result.rejectionReason = "canceled";
            } else {
                if (conversion.status == LegacySpreadsheetConversionStatus::CleanupFailed) {
                    result.rejectionReason = "staging_cleanup_failed";
                }
                appendDiagnostic(result.diagnostic, conversion.message);
                appendDiagnostic(result.diagnostic, conversion.diagnostic);
            }
            ++result.failedLegacyXls;
            return false;
        }
        ++result.convertedXls;
        if (!conversion.diagnostic.empty()) {
            result.warning = true;
            appendDiagnostic(result.diagnostic, conversion.diagnostic);
        }
        result.files.push_back({conversion.outputPath, std::move(consolidationSources), true});
        return true;
    }

    std::filesystem::path
    ImportFileStager::stagedDestination(const StagedDestinationRequest& request) const {
        std::filesystem::path candidateName = request.source.stem();
        candidateName += "_";
        candidateName += request.batchPrefix;
        candidateName += "_";
        candidateName += std::to_string(request.fileIndex);
        candidateName += request.source.extension();
        return inputFolder_ / candidateName;
    }

} // namespace ssa::infra::importing
