#pragma once

#include <algorithm>

namespace ssa::ports {

    inline constexpr int kMinDetailsPanelWidth = 320;
    inline constexpr int kMaxDetailsPanelWidth = 920;
    inline constexpr int kDefaultDetailsPanelWidth = 520;

    [[nodiscard]] inline int clampDetailsPanelWidth(const int value) {
        return std::clamp(value, kMinDetailsPanelWidth, kMaxDetailsPanelWidth);
    }

} // namespace ssa::ports
