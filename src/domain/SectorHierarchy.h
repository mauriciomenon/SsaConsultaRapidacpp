#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    struct SectorDivision {
        std::string_view key;
        std::span<const std::string_view> sectors;
    };

    class SectorHierarchy final {
      public:
        [[nodiscard]] static std::span<const SectorDivision> divisions();
        [[nodiscard]] static std::vector<std::string> sectorsForDivision(std::string_view key);
        [[nodiscard]] static std::vector<std::string>
        orderedSectors(std::span<const std::string> sectors);
        [[nodiscard]] static std::string divisionForSector(std::string_view sector);
        [[nodiscard]] static bool containsDivision(std::string_view key);
    };

} // namespace ssa::domain
