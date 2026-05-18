#pragma once

#include "ports/IFilterPresetStore.h"

#include <QJsonDocument>

namespace ssa::infra::preferences {

    class FilterPresetJsonCodec final {
      public:
        [[nodiscard]] ports::FilterPresetSnapshot
        snapshotFromDocument(const QJsonDocument& document) const;
        [[nodiscard]] QJsonDocument
        documentFromSnapshot(const ports::FilterPresetSnapshot& snapshot) const;
    };

} // namespace ssa::infra::preferences
