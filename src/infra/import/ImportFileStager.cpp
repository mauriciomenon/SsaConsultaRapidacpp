#include "infra/import/ImportFileStager.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string_view>

namespace ssa::infra::importing {

    namespace {

        constexpr std::size_t kMaxImportFiles = 64;
        constexpr std::uintmax_t kMaxImportFileBytes = 128ULL * 1024ULL * 1024ULL;
        constexpr std::uintmax_t kMaxImportBatchBytes = 1024ULL * 1024ULL * 1024ULL;

        std::string lowercaseExtension(const std::filesystem::path& path) {
            auto extension = path.extension().string();
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
            return path.filename().string().starts_with("~$");
        }

        bool isSafeFilename(const std::filesystem::path& filename) {
            return !filename.empty() && filename == filename.filename() && filename != "." &&
                   filename != "..";
        }

        std::string batchPrefix() {
            static std::atomic_uint64_t sequence{0};
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            return std::to_string(
                       std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()) +
                   "_" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        }

        std::string preflightFiles(const std::vector<std::filesystem::path>& files) {
            if (files.size() > kMaxImportFiles) {
                return "too_many_files max=64";
            }
            std::uintmax_t totalBytes = 0;
            for (const auto& file : files) {
                std::error_code error;
                const auto fileBytes = std::filesystem::file_size(file, error);
                if (error) {
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

    } // namespace

    ImportFileStager::ImportFileStager(std::filesystem::path inputFolder,
                                       LegacySpreadsheetConverter legacyConverter)
        : inputFolder_(std::move(inputFolder)), legacyConverter_(std::move(legacyConverter)) {}

    ImportStagingResult
    ImportFileStager::stageExternalFiles(const std::vector<std::filesystem::path>& files) const {
        ImportStagingResult result;
        result.rejectionReason = preflightFiles(files);
        if (!result.rejectionReason.empty()) {
            return result;
        }
        std::error_code error;
        std::filesystem::create_directories(inputFolder_, error);
        if (error) {
            result.failedCopies = files.size();
            return result;
        }

        const auto prefix = batchPrefix();
        std::size_t fileIndex = 0;
        for (const auto& source : files) {
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
                stageLegacyFile({source, xlsxDestination}, result);
                continue;
            }
            if (!isXlsxFile(source)) {
                ++result.unsupported;
                continue;
            }
            const auto destination = stagedDestination({source, prefix, fileIndex});
            ++fileIndex;
            std::filesystem::copy_file(source, destination,
                                       std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                ++result.failedCopies;
                error.clear();
                continue;
            }
            result.xlsxFiles.push_back(destination);
        }
        return result;
    }

    ImportStagingResult ImportFileStager::stageInputFiles() const {
        ImportStagingResult result;
        std::error_code error;
        const bool inputExists = std::filesystem::exists(inputFolder_, error);
        if (error) {
            result.failedCopies = 1;
            return result;
        }
        if (!inputExists) {
            return result;
        }
        const bool inputIsDirectory = std::filesystem::is_directory(inputFolder_, error);
        if (error || !inputIsDirectory) {
            result.failedCopies = 1;
            return result;
        }
        std::vector<std::filesystem::path> candidates;
        std::filesystem::directory_iterator iterator{inputFolder_, error};
        if (error) {
            result.failedCopies = 1;
            return result;
        }
        for (const auto& entry : iterator) {
            if (!entry.is_regular_file(error) || error) {
                error.clear();
                continue;
            }
            candidates.push_back(entry.path());
        }
        std::ranges::sort(candidates);

        std::vector<std::filesystem::path> importCandidates;
        std::ranges::copy_if(
            candidates, std::back_inserter(importCandidates), [](const auto& path) {
                return !isExcelLockFile(path) && (isLegacyXlsFile(path) || isXlsxFile(path));
            });
        result.rejectionReason = preflightFiles(importCandidates);
        if (!result.rejectionReason.empty()) {
            return result;
        }

        for (const auto& path : candidates) {
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
                stageLegacyFile({path, destination}, result);
                continue;
            }
            if (isXlsxFile(path)) {
                result.xlsxFiles.push_back(path);
                continue;
            }
            ++result.unsupported;
        }
        std::ranges::sort(result.xlsxFiles);
        return result;
    }

    bool ImportFileStager::stageLegacyFile(const LegacyStageRequest& request,
                                           ImportStagingResult& result) const {
        ++result.legacyXls;
        const auto conversion = legacyConverter_.convertToXlsx(request.source, request.destination);
        if (!conversion.ok()) {
            ++result.failedLegacyXls;
            return false;
        }
        ++result.convertedXls;
        result.xlsxFiles.push_back(conversion.outputPath);
        return true;
    }

    std::filesystem::path
    ImportFileStager::stagedDestination(const StagedDestinationRequest& request) const {
        std::filesystem::path candidateName{request.source.stem()};
        candidateName += "_";
        candidateName += request.batchPrefix;
        candidateName += "_";
        candidateName += std::to_string(request.fileIndex);
        candidateName += request.source.extension();
        return inputFolder_ / candidateName;
    }

} // namespace ssa::infra::importing
