#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QJsonDocument>

namespace ssa::infra::preferences {

    class UserPreferencesJsonCodec final {
      public:
        [[nodiscard]] ports::UserPreferencesSnapshot
        snapshotFromDocument(const QJsonDocument& document) const;

        [[nodiscard]] QJsonDocument
        documentFromSnapshot(const ports::UserPreferencesSnapshot& snapshot) const;
    };

} // namespace ssa::infra::preferences
