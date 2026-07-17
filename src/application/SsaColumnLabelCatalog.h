#pragma once

#include <span>
#include <string_view>

namespace ssa::application {

    struct SsaColumnLabel final {
        std::string_view key;
        std::string_view label;
        std::string_view labelFull;
    };

    class SsaColumnLabelCatalog final {
      public:
        [[nodiscard]] static std::span<const SsaColumnLabel> all() noexcept;
        [[nodiscard]] static const SsaColumnLabel* find(std::string_view key) noexcept;
    };

} // namespace ssa::application
