#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace ssa::infra::importing {

    class SsaSpreadsheetHeaderCatalog final {
      public:
        [[nodiscard]] static std::optional<std::string>
        canonicalColumnForHeader(const std::string& header);
        [[nodiscard]] static std::size_t sourceAliasCount();
    };

} // namespace ssa::infra::importing
