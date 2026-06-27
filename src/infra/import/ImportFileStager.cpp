#include "infra/import/ImportFileStager.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string_view>

namespace ssa::infra::importing {

    namespace {

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

    } // namespace

    ImportFileStager::ImportFileStager(std::filesystem::path inputFolder,
                                       LegacySpreadsheetConverter legacyConverter)
        : inputFolder_(std::move(inputFolder)), legacyConverter_(std::move(legacyConverter)) {}

    ImportStagingResult
    ImportFileStager::stageExternalFiles(const std::vector<std::filesystem::path>& files) const {
        ImportStagingResult result;
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
        if (!std::filesystem::exists(inputFolder_)) {
            return result;
        }
        std::vector<std::filesystem::path> candidates;
        for (const auto& entry : std::filesystem::directory_iterator(inputFolder_)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            candidates.push_back(entry.path());
        }
        std::ranges::sort(candidates);

        for (const auto& path : candidates) {
            if (isExcelLockFile(path)) {
                ++result.unsupported;
                continue;
            }
            if (isLegacyXlsFile(path)) {
                auto destination = path;
                destination.replace_extension(".xlsx");
                if (std::filesystem::exists(destination)) {
                    ++result.legacyXls;
                    continue;
                }
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
