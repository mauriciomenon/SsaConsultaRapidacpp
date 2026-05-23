#pragma once

#include <optional>
#include <string>

namespace ssa::infra::importing {

    class SsaSpreadsheetHeaderCatalog final {
      public:
        [[nodiscard]] static std::optional<std::string>
        canonicalColumnForHeader(const std::string& normalizedHeader);
    };

} // namespace ssa::infra::importing
