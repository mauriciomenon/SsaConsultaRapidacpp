#pragma once

#include "infra/import/ImportConsolidation.h"

#include <filesystem>
#include <stop_token>
#include <vector>

namespace ssa::infra::importing {

    class ImportFileConsolidator final {
      public:
        explicit ImportFileConsolidator(std::filesystem::path inputFolder);

        [[nodiscard]] ImportConsolidationPlan plan(const std::vector<ImportManifestEntry>& manifest,
                                                   const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const std::vector<ImportManifestEntry>& manifest,
                    const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const ImportConsolidationPlan& plan,
                    const std::stop_token& stopToken = {}) const;

      private:
        std::filesystem::path inputFolder_;
    };

} // namespace ssa::infra::importing
