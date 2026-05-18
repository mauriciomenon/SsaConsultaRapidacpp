#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QJsonObject>

namespace ssa::infra::preferences {

    class FilterPreferencesJsonCodec final {
      public:
        [[nodiscard]] ports::FilterPreferencesSnapshot
        filtersFromObject(const QJsonObject& root,
                          ports::FilterPreferencesSnapshot baseSnapshot = {}) const;
        void writeFilters(QJsonObject& root, const ports::FilterPreferencesSnapshot& filters) const;
    };

} // namespace ssa::infra::preferences
