#pragma once

#include <array>
#include <string_view>

namespace ssa::domain {

    inline constexpr std::array<std::string_view, 8> kOrderedPriorityValues{
        "IEE3", "IEE1", "IEE2", "IEE4", "MEL1", "MEL2", "MEL3", "MEL4"};

    [[nodiscard]] bool isPriorityColumnValue(std::string_view value);
    [[nodiscard]] bool usesPriorityValueOrder(std::string_view columnKey);
    [[nodiscard]] bool columnValueLessForDisplay(std::string_view left, std::string_view right);

} // namespace ssa::domain
