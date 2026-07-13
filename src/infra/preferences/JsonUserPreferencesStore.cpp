#include "infra/preferences/JsonUserPreferencesStore.h"

#include "domain/ColumnCatalog.h"
#include "infra/preferences/JsonPersistenceSupport.h"
#include "infra/preferences/UserPreferencesJsonCodec.h"
#include "qt/FilesystemPath.h"

#include <QJsonDocument>

#include <stdexcept>
#include <system_error>
#include <utility>

namespace ssa::infra::preferences {

    JsonUserPreferencesStore::JsonUserPreferencesStore(std::filesystem::path path)
        : path_(std::move(path)) {}

    ports::UserPreferencesSnapshot
    JsonUserPreferencesStore::load(const std::stop_token stopToken) const {
        json_persistence::throwIfCanceled(stopToken);
        ports::UserPreferencesSnapshot snapshot;
        snapshot.visibleColumns = domain::ColumnCatalog::defaultVisibleKeys();

        if (!std::filesystem::exists(path_)) {
            return snapshot;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(
            json_persistence::readBounded(path_, "preferences file", stopToken), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw std::runtime_error("invalid preferences json: " + qt::toUtf8(path_));
        }
        return UserPreferencesJsonCodec{}.snapshotFromDocument(document);
    }

    void JsonUserPreferencesStore::save(const ports::UserPreferencesSnapshot& snapshot,
                                        const std::stop_token stopToken) const {
        json_persistence::throwIfCanceled(stopToken);
        std::error_code error;
        std::filesystem::create_directories(path_.parent_path(), error);
        if (error) {
            throw std::runtime_error("cannot create preferences directory: " + error.message());
        }

        const auto document = UserPreferencesJsonCodec{}.documentFromSnapshot(snapshot);
        json_persistence::writeAtomically(path_, document.toJson(QJsonDocument::Compact),
                                          "preferences file", stopToken);
    }

} // namespace ssa::infra::preferences
