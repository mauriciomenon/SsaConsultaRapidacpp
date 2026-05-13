#pragma once

#include <algorithm>

namespace ssa::ports {

    inline constexpr int kMinDetailsPanelWidth = 280;
    inline constexpr int kMaxDetailsPanelWidth = 900;
    inline constexpr int kDefaultDetailsPanelWidth = 360;

    [[nodiscard]] inline int clampDetailsPanelWidth(const int value) {
        return std::clamp(value, kMinDetailsPanelWidth, kMaxDetailsPanelWidth);
    }

} // namespace ssa::ports
