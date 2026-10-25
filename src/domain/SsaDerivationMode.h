#pragma once

#include "domain/SsaTypes.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace ssa::domain {

    // Persisted and QML keys of each derivation filter mode. Unknown keys mean All.
    inline constexpr std::array<std::pair<DerivationFilterMode, std::string_view>, 5>
        kDerivationModeKeys{{
            {DerivationFilterMode::All, "all"},
            {DerivationFilterMode::RootOnly, "root"},
            {DerivationFilterMode::DerivedOnly, "derived"},
            {DerivationFilterMode::HasDerived, "has"},
            {DerivationFilterMode::ClosedDerived, "closed"},
        }};

    [[nodiscard]] inline DerivationFilterMode derivationModeFromKey(const std::string_view key) {
        const auto* found = std::ranges::find(
            kDerivationModeKeys, key, &std::pair<DerivationFilterMode, std::string_view>::second);
        return found == kDerivationModeKeys.end() ? DerivationFilterMode::All : found->first;
    }

    [[nodiscard]] inline std::string_view derivationModeKey(const DerivationFilterMode mode) {
        const auto* found = std::ranges::find(
            kDerivationModeKeys, mode, &std::pair<DerivationFilterMode, std::string_view>::first);
        return found == kDerivationModeKeys.end() ? kDerivationModeKeys.front().second
                                                  : found->second;
    }

    [[nodiscard]] inline std::string normalizedDerivationMode(const std::string_view value) {
        return std::string{derivationModeKey(derivationModeFromKey(value))};
    }

} // namespace ssa::domain
