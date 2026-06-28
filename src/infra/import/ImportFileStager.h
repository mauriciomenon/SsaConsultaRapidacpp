#pragma once

#include "infra/import/LegacySpreadsheetConverter.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::infra::importing {

    struct ImportStagingResult {
        std::vector<std::filesystem::path> xlsxFiles;
        std::size_t legacyXls{0};
        std::size_t convertedXls{0};
        std::size_t failedLegacyXls{0};
        std::size_t unsupported{0};
        std::size_t failedCopies{0};
    };

    class ImportFileStager final {
      public:
        explicit ImportFileStager(std::filesystem::path inputFolder,
                                  LegacySpreadsheetConverter legacyConverter = {});

        [[nodiscard]] ImportStagingResult
        stageExternalFiles(const std::vector<std::filesystem::path>& files) const;
        [[nodiscard]] ImportStagingResult stageInputFiles() const;

      private:
        struct LegacyStageRequest {
            const std::filesystem::path& source;
            const std::filesystem::path& destination;
        };

        struct StagedDestinationRequest {
            const std::filesystem::path& source;
            std::string_view batchPrefix;
            std::size_t fileIndex{0};
        };

        bool stageLegacyFile(const LegacyStageRequest& request, ImportStagingResult& result) const;
        [[nodiscard]] std::filesystem::path
        stagedDestination(const StagedDestinationRequest& request) const;

        std::filesystem::path inputFolder_;
        LegacySpreadsheetConverter legacyConverter_;
    };

} // namespace ssa::infra::importing
