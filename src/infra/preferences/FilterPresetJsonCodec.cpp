#include "infra/preferences/FilterPresetJsonCodec.h"

#include "infra/preferences/FilterPreferencesJsonCodec.h"

#include <QJsonObject>

#include <stdexcept>

namespace ssa::infra::preferences {

    ports::FilterPresetSnapshot
    FilterPresetJsonCodec::snapshotFromDocument(const QJsonDocument& document) const {
        if (!document.isObject()) {
            throw std::runtime_error("invalid filter preset json");
        }

        const QJsonObject root = document.object();
        ports::FilterPresetSnapshot snapshot;
        snapshot.filters = FilterPreferencesJsonCodec{}.filtersFromObject(root, snapshot.filters);
        return snapshot;
    }

    QJsonDocument
    FilterPresetJsonCodec::documentFromSnapshot(const ports::FilterPresetSnapshot& snapshot) const {
        QJsonObject root;
        root.insert("schema_version", 1);
        FilterPreferencesJsonCodec{}.writeFilters(root, snapshot.filters);
        return QJsonDocument(root);
    }

} // namespace ssa::infra::preferences
