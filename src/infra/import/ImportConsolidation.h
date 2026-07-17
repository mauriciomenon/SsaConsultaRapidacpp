#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    struct ImportManifestEntry {
        std::vector<std::filesystem::path> sources;
        bool hasValidRows = false;
        std::filesystem::path destinationFilename;
    };

    struct ImportConsolidationMove {
        std::filesystem::path source;
        std::filesystem::path destination;
        bool hasValidRows = false;
        std::string sourceIdentity;
        std::optional<std::uintmax_t> sourceSize;
    };

    struct ImportConsolidationPlanEntry {
        std::vector<ImportConsolidationMove> moves;
    };

    struct ImportConsolidationPlan {
        std::vector<ImportConsolidationPlanEntry> entries;
        bool canceled = false;
        std::string error;
    };

    struct ImportConsolidationMoveResult {
        bool completed = false;
        bool moved = false;
        bool failed = false;
    };

    struct ImportConsolidationEntryResult {
        std::vector<ImportConsolidationMoveResult> moves;
        std::size_t completed = 0;
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

} // namespace ssa::infra::importing
