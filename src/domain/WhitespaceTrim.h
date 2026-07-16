#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ssa::domain {

    [[nodiscard]] inline std::string trimWhitespace(const std::string_view value) {
        const auto begin = std::ranges::find_if_not(
            value, [](const unsigned char ch) { return std::isspace(ch) != 0; });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char ch) {
                             return std::isspace(ch) != 0;
                         }).base();
        return begin < end ? std::string{begin, end} : std::string{};
    }

} // namespace ssa::domain
