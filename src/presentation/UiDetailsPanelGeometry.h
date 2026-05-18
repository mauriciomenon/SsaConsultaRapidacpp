#pragma once

#include "ports/UserPreferenceDefaults.h"

#include <algorithm>

namespace ssa::presentation::details_layout {

    struct DetailsPanelGeometry {
        int minimumWidth;
        int preferredWidth;
        int maximumWidth;
    };

    inline DetailsPanelGeometry computeDetailsPanelGeometry(const int viewportWidth) {
        const int normalizedViewport = viewportWidth > 0 ? viewportWidth : 1;
        constexpr int kDetailsMinRatioPercent = 30;
        constexpr int kDetailsPrefRatioPercent = 40;
        constexpr int kDetailsMaxRatioPercent = 50;

        const int minByRatio = (normalizedViewport * kDetailsMinRatioPercent) / 100;
        const int maxByRatio = (normalizedViewport * kDetailsMaxRatioPercent) / 100;
        const int preferredByRatio = (normalizedViewport * kDetailsPrefRatioPercent) / 100;

        const int minimum = std::max(ports::kMinDetailsPanelWidth, minByRatio);
        const int maximum = std::max(minimum, std::min(ports::kMaxDetailsPanelWidth, maxByRatio));
        const int preferred = std::max(minimum, std::min(maximum, preferredByRatio));

        return {minimum, preferred, maximum};
    }

} // namespace ssa::presentation::details_layout
