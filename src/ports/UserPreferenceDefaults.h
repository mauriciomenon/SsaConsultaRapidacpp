#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace ssa::ports {

    inline constexpr int kMinDetailsPanelWidth = 320;
    inline constexpr int kMaxDetailsPanelWidth = 1200;
    inline constexpr int kDefaultDetailsPanelWidth = 450;
    inline constexpr std::array<std::string_view, 15> kThemeValues{
        "system",     "light", "grayscale",   "windows7",       "classico",
        "gruvbox",    "dark",  "dracula",     "solarized-dark", "solarized-light",
        "mint-light", "paper", "tokyo-night", "catppuccin",     "nord"};
    inline constexpr std::array<std::string_view, 3> kDensityValues{"compact", "normal",
                                                                    "comfortable"};

    [[nodiscard]] inline bool isThemeValid(const std::string_view theme) {
        return std::ranges::any_of(
            kThemeValues, [theme](const std::string_view value) { return value == theme; });
    }

    [[nodiscard]] inline bool isDensityValid(const std::string_view density) {
        return std::ranges::any_of(
            kDensityValues, [density](const std::string_view value) { return value == density; });
    }

    [[nodiscard]] inline int clampDetailsPanelWidth(const int value) {
        return std::clamp(value, kMinDetailsPanelWidth, kMaxDetailsPanelWidth);
    }

} // namespace ssa::ports
