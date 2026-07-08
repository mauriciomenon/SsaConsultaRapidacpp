#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QJsonDocument>

namespace ssa::infra::preferences {

    class UserPreferencesJsonCodec final {
      public:
        [[nodiscard]] static ports::UserPreferencesSnapshot
        snapshotFromDocument(const QJsonDocument& document);

        [[nodiscard]] static QJsonDocument
        documentFromSnapshot(const ports::UserPreferencesSnapshot& snapshot);
    };

} // namespace ssa::infra::preferences
