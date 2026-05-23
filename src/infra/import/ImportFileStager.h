#pragma once

#include "infra/import/LegacySpreadsheetConverter.h"

#include <filesystem>
#include <string>
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
        bool stageLegacyFile(const std::filesystem::path& source,
                             const std::filesystem::path& destination,
                             ImportStagingResult& result) const;
        [[nodiscard]] std::filesystem::path stagedDestination(const std::filesystem::path& source,
                                                              const std::string& batchPrefix,
                                                              std::size_t fileIndex) const;

        std::filesystem::path inputFolder_;
        LegacySpreadsheetConverter legacyConverter_;
    };

} // namespace ssa::infra::importing
