#include "infra/import/ImportFileStager.h"

#include "infra/import/CancelableFileCopy.h"
#include "infra/import/ImportPathValidation.h"
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
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxImportFilesPerBatch = 64;
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

    ImportFileStager::ImportFileStager(std::filesystem::path inputFolder,
                                       FileCopyFirstChunkWrittenHook afterFirstChunkWritten)
        : inputFolder_(std::move(inputFolder)),
          afterFirstChunkWritten_(std::move(afterFirstChunkWritten)) {}

    ImportStagingResult
    ImportFileStager::stageExternalFiles(const std::vector<std::filesystem::path>& files,
                                         const std::stop_token& stopToken) const {
        ImportStagingResult result;
        result.discovered = files.size();
        for (const auto& source : files) {
            recordDiscoveredFile(result, source);
        }
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

        std::size_t fileIndex = 0;
        std::size_t summaryIndex = 0;
        for (std::size_t batchStart = 0; batchStart < files.size();
             batchStart += kMaxImportFilesPerBatch) {
            const auto prefix = batchPrefix();
            const auto batchEnd = (std::min)(files.size(), batchStart + kMaxImportFilesPerBatch);
            for (std::size_t sourceIndex = batchStart; sourceIndex < batchEnd; ++sourceIndex) {
                const auto& source = files[sourceIndex];
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
                if (!isSafeImportFilename(filename)) {
                    ++result.failedCopies;
                    continue;
                }
                if (isLegacyXlsFile(source) || !isXlsx) {
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
                const auto copy =
                    copyFileAtomically({source, destination, afterFirstChunkWritten_}, stopToken);
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
        result.rejectionReason = preflightFiles(importCandidates, stopToken, result.diagnostic);
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
