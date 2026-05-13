#pragma once

#include <QVariant>

#include <cstddef>
#include <vector>

namespace ssa::presentation {

    struct SsaTableDisplayValues {
        std::vector<QVariant> values;
        std::size_t rowCount{0};
        std::size_t columnCount{0};

        [[nodiscard]] bool hasValidShape() const;
    };

    class SsaTableDisplayCache final {
      public:
        void replace(SsaTableDisplayValues values);
        [[nodiscard]] QVariant value(std::size_t row, std::size_t column) const;

      private:
        [[nodiscard]] std::size_t offset(std::size_t row, std::size_t column) const;

        std::vector<QVariant> values_;
        std::size_t rowCount_{0};
        std::size_t columnCount_{0};
    };

} // namespace ssa::presentation
