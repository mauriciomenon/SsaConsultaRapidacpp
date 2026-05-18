#pragma once

#include <string_view>

namespace ssa::domain {

    [[nodiscard]] bool isPriorityColumnValue(std::string_view value);

} // namespace ssa::domain
