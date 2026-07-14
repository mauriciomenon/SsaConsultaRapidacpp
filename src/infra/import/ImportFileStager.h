#pragma once

#include "infra/import/LegacySpreadsheetConverter.h"

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
    };

    struct ImportManifestEntry {
        std::vector<std::filesystem::path> sources;
        bool hasValidRows = false;
    };

    struct ImportConsolidationResult {
        std::size_t moved = 0;
        std::size_t noSurvivor = 0;
        std::size_t failed = 0;
        bool canceled = false;
        std::string error;
    };

    struct ImportStagingResult {
        std::vector<StagedImportFile> files;
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
        explicit ImportFileStager(std::filesystem::path inputFolder,
                                  LegacySpreadsheetConverter legacyConverter = {});

        [[nodiscard]] ImportStagingResult
        stageExternalFiles(const std::vector<std::filesystem::path>& files,
                           const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportStagingResult stageInputFiles(const std::stop_token& stopToken = {},
                                                          bool includeProcessed = false) const;
        [[nodiscard]] std::string discardOwnedArtifacts(const ImportStagingResult& staging) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const std::vector<ImportManifestEntry>& manifest,
                    const std::stop_token& stopToken = {}) const;

      private:
        struct LegacyStageRequest {
            const std::filesystem::path& source;
            const std::filesystem::path& destination;
        };

        struct StagedDestinationRequest {
            std::filesystem::path source;
            std::string_view batchPrefix;
            std::size_t fileIndex = 0;
        };

        bool stageLegacyFile(const LegacyStageRequest& request,
                             std::vector<std::filesystem::path> consolidationSources,
                             ImportStagingResult& result, const std::stop_token& stopToken) const;
        [[nodiscard]] std::filesystem::path
        stagedDestination(const StagedDestinationRequest& request) const;

        std::filesystem::path inputFolder_;
        LegacySpreadsheetConverter legacyConverter_;
    };

} // namespace ssa::infra::importing
