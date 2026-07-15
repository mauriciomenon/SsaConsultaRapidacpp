#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::infra::importing {

    struct StagedImportFile {
        std::filesystem::path workbookPath;
        std::vector<std::filesystem::path> consolidationSources;
        bool ownedByStager = false;
        std::string originalFilename;
        std::string sourceModifiedTimestamp;
        std::size_t summaryIndex = 0;
    };

    struct ImportManifestEntry {
        std::vector<std::filesystem::path> sources;
        bool hasValidRows = false;
    };

    struct ImportConsolidationEntryResult {
        std::size_t moved = 0;
        std::size_t noSurvivor = 0;
        std::size_t failed = 0;
    };

    struct ImportConsolidationResult {
        std::vector<ImportConsolidationEntryResult> entries;
        std::size_t moved = 0;
        std::size_t noSurvivor = 0;
        std::size_t failed = 0;
        bool canceled = false;
        std::string error;
    };

    struct ImportStagingResult {
        std::vector<StagedImportFile> files;
        std::vector<std::string> discoveredXlsxSources;
        std::size_t discovered = 0;
        std::size_t legacyXls = 0;
        std::size_t convertedXls = 0;
        std::size_t failedLegacyXls = 0;
        std::size_t unsupported = 0;
        std::size_t failedCopies = 0;
        bool warning = false;
        bool operationalFailure = false;
        std::string rejectionReason;
        std::string diagnostic;
    };

    class ImportFileStager final {
      public:
        explicit ImportFileStager(std::filesystem::path inputFolder);

        [[nodiscard]] ImportStagingResult
        stageExternalFiles(const std::vector<std::filesystem::path>& files,
                           const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportStagingResult
        validateInputDirectory(const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportStagingResult stageInputFiles(const std::stop_token& stopToken = {},
                                                          bool includeProcessed = false) const;
        [[nodiscard]] std::string discardOwnedArtifacts(const ImportStagingResult& staging) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const std::vector<ImportManifestEntry>& manifest,
                    const std::stop_token& stopToken = {}) const;

      private:
        struct StagedDestinationRequest {
            std::filesystem::path source;
            std::string_view batchPrefix;
            std::size_t fileIndex = 0;
        };

        [[nodiscard]] std::filesystem::path
        stagedDestination(const StagedDestinationRequest& request) const;

        std::filesystem::path inputFolder_;
    };

} // namespace ssa::infra::importing
