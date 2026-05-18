#pragma once

#include <QVariantList>

namespace ssa::presentation {

    class AdvancedTextFilterRowModelFactory final {
      public:
        [[nodiscard]] QVariantList buildRows() const;
    };

} // namespace ssa::presentation
