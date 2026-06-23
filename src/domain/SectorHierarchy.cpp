#include "domain/SectorHierarchy.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace ssa::domain {

    namespace {
        constexpr std::array<std::string_view, 4> kSmmeSectors{"MEL1", "MEL2", "MEL3", "MEL4"};
        constexpr std::array<std::string_view, 4> kSminSectors{"IEE1", "IEE2", "IEE3", "IEE4"};
        constexpr std::array<std::string_view, 4> kSmilSectors{"ILA1", "ILA2", "ILA3", "ILA4"};
        constexpr std::array<std::string_view, 4> kSmmgSectors{"MEG1", "MEG2", "MEG3", "MEG4"};

        constexpr std::array<SectorDivision, 4> kDivisions{{
            {"SMME", kSmmeSectors},
            {"SMIN", kSminSectors},
            {"SMIL", kSmilSectors},
            {"SMMG", kSmmgSectors},
        }};

        [[nodiscard]] std::string uppercaseCopy(const std::string_view value) {
            std::string result{value};
            std::ranges::transform(result, result.begin(), [](const unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            return result;
        }

        [[nodiscard]] bool isKnownSector(const std::string_view sector) {
            return std::ranges::any_of(kDivisions, [sector](const auto& division) {
                return std::ranges::find(division.sectors, sector) != division.sectors.end();
            });
        }
    } // namespace

    std::span<const SectorDivision> SectorHierarchy::divisions() {
        return kDivisions;
    }

    std::vector<std::string> SectorHierarchy::sectorsForDivision(const std::string_view key) {
        const auto normalized = uppercaseCopy(key);
        const auto it = std::ranges::find_if(
            kDivisions, [&normalized](const auto& division) { return division.key == normalized; });
        if (it == kDivisions.end()) {
            return {};
        }
        std::vector<std::string> result;
        result.reserve(it->sectors.size());
        for (const auto sector : it->sectors) {
            result.emplace_back(sector);
        }
        return result;
    }

    std::vector<std::string>
    SectorHierarchy::orderedSectors(const std::span<const std::string> sectors) {
        std::vector<std::string> result;
        result.reserve(sectors.size());
        for (const auto& sector : sectors) {
            auto normalized = uppercaseCopy(sector);
            if (!normalized.empty() && std::ranges::find(result, normalized) == result.end()) {
                result.push_back(std::move(normalized));
            }
        }

        std::vector<std::string> ordered;
        ordered.reserve(result.size());
        for (const auto& division : kDivisions) {
            for (const auto sector : division.sectors) {
                if (std::ranges::find(result, sector) != result.end()) {
                    ordered.emplace_back(sector);
                }
            }
        }
        for (const auto& sector : result) {
            if (!isKnownSector(sector)) {
                ordered.push_back(sector);
            }
        }
        return ordered;
    }

    std::string SectorHierarchy::divisionForSector(const std::string_view sector) {
        const auto normalized = uppercaseCopy(sector);
        for (const auto& division : kDivisions) {
            if (std::ranges::find(division.sectors, normalized) != division.sectors.end()) {
                return std::string{division.key};
            }
        }
        return {};
    }

    bool SectorHierarchy::containsDivision(const std::string_view key) {
        const auto normalized = uppercaseCopy(key);
        return std::ranges::any_of(
            kDivisions, [&normalized](const auto& division) { return division.key == normalized; });
    }

} // namespace ssa::domain
