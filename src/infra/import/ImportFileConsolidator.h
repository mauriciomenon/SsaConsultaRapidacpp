#pragma once

#include "infra/import/ImportConsolidation.h"

#include <cstddef>
#include <filesystem>
#include <stop_token>
#include <vector>

namespace ssa::infra::importing {

    inline constexpr std::size_t kMaxConsolidationDestinationAttempts = 10'000;

    // Where a consolidated copy goes when earlier attempts are taken: name, then name__1,
    // name__2, ... A caller moves with a no-replace publisher and takes the next on a clash.
    [[nodiscard]] std::filesystem::path
    consolidationDestinationCandidate(const std::filesystem::path& directory,
                                      const std::filesystem::path& filename, std::size_t attempt);

    class ImportFileConsolidator final {
      public:
        explicit ImportFileConsolidator(std::filesystem::path inputFolder);

        [[nodiscard]] ImportConsolidationPlan plan(const std::vector<ImportManifestEntry>& manifest,
                                                   const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const std::vector<ImportManifestEntry>& manifest,
                    const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ImportConsolidationResult
        consolidate(const ImportConsolidationPlan& consolidationPlan,
                    const std::stop_token& stopToken = {}) const;

      private:
        std::filesystem::path inputFolder_;
    };

} // namespace ssa::infra::importing
