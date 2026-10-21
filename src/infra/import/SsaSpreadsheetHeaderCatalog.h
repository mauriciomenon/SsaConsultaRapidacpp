#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ssa::infra::importing {

    class SsaSpreadsheetHeaderCatalog final {
      public:
        [[nodiscard]] static std::optional<std::string>
        canonicalColumnForHeader(const std::string& header);
        // Lowercase, accent-free, single-spaced header text used for every header match.
        [[nodiscard]] static std::string normalizedHeader(std::string_view header);
        [[nodiscard]] static std::size_t sourceLabelCount();
    };

} // namespace ssa::infra::importing
