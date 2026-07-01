#pragma once

#include <string_view>

namespace ssa::domain {

    [[nodiscard]] bool isPriorityColumnValue(std::string_view value);
    [[nodiscard]] bool columnValueLessForDisplay(std::string_view left, std::string_view right);

} // namespace ssa::domain
