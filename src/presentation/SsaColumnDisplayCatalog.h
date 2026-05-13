#pragma once

#include "domain/ColumnCatalog.h"

#include <string>
#include <vector>

namespace ssa::presentation {

    struct SsaDisplayColumn {
        std::string key;
        std::string label;
        domain::ColumnType type{domain::ColumnType::Text};
        int defaultWidth{132};
    };

    class SsaColumnDisplayCatalog final {
      public:
        [[nodiscard]] SsaDisplayColumn resolve(const std::string& key) const;
        [[nodiscard]] std::vector<SsaDisplayColumn>
        resolveAll(const std::vector<std::string>& keys) const;
    };

} // namespace ssa::presentation
