#pragma once

#include "domain/ColumnCatalog.h"

#include <string>
#include <string_view>
#include <vector>

namespace ssa::presentation {

    inline constexpr int kDefaultDisplayColumnWidth = 132;

    struct SsaDisplayColumn {
        std::string key;
        std::string label;
        std::string labelFull;
        domain::ColumnType type{domain::ColumnType::Text};
        bool defaultVisible{false};
        int defaultWidth{kDefaultDisplayColumnWidth};
    };

    class SsaColumnDisplayCatalog final {
      public:
        [[nodiscard]] SsaDisplayColumn resolve(const std::string& key) const;
        [[nodiscard]] std::vector<SsaDisplayColumn>
        resolveAll(const std::vector<std::string>& keys) const;
        [[nodiscard]] std::vector<SsaDisplayColumn> all() const;
        [[nodiscard]] std::string advancedFilterLabel(std::string_view key) const;
        [[nodiscard]] std::string advancedFilterShortLabel(std::string_view key) const;
    };

} // namespace ssa::presentation
