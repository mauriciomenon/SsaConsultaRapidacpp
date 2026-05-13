#include "presentation/SsaTableDisplayCache.h"

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    bool SsaTableDisplayValues::hasValidShape() const {
        return values.size() == rowCount * columnCount;
    }

    void SsaTableDisplayCache::replace(SsaTableDisplayValues values) {
        if (!values.hasValidShape()) {
            throw std::invalid_argument("invalid table display values shape");
        }
        rowCount_ = values.rowCount;
        columnCount_ = values.columnCount;
        values_ = std::move(values.values);
    }

    QVariant SsaTableDisplayCache::value(const std::size_t row, const std::size_t column) const {
        if (row >= rowCount_ || column >= columnCount_) {
            return {};
        }
        const auto key = offset(row, column);
        if (key >= values_.size()) {
            return {};
        }
        return values_[key];
    }

    std::size_t SsaTableDisplayCache::offset(const std::size_t row,
                                             const std::size_t column) const {
        return row * columnCount_ + column;
    }

} // namespace ssa::presentation
