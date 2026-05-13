#pragma once

#include <string>
#include <string_view>

namespace ssa::query {

    [[nodiscard]] std::string quoteColumnIdentifier(const std::string& key);
    [[nodiscard]] std::string quoteTableIdentifier(const std::string& name);
    [[nodiscard]] std::string uppercaseCopy(std::string_view value);

} // namespace ssa::query
