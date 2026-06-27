#pragma once

#include "domain/ColumnCatalog.h"

#include <string>
#include <vector>

namespace ssa::presentation {

    inline constexpr int kDefaultDisplayColumnWidth = 132;

    struct SsaDisplayColumn {
        std::string key;
        std::string label;
        domain::ColumnType type{domain::ColumnType::Text};
        int defaultWidth{kDefaultDisplayColumnWidth};
    };

    class SsaColumnDisplayCatalog final {
      public:
        [[nodiscard]] SsaDisplayColumn resolve(const std::string& key) const;
        [[nodiscard]] std::vector<SsaDisplayColumn>
        resolveAll(const std::vector<std::string>& keys) const;
    };

} // namespace ssa::presentation
