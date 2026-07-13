#include "infra/preferences/JsonFilterPresetStore.h"

#include "infra/preferences/FilterPresetJsonCodec.h"
#include "infra/preferences/JsonPersistenceSupport.h"
#include "qt/FilesystemPath.h"

#include <QJsonDocument>

#include <stdexcept>
#include <system_error>

namespace ssa::infra::preferences {

    namespace {
        void ensureParentDirectory(const std::filesystem::path& path) {
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            if (error) {
                throw std::runtime_error("cannot create preset directory: " + error.message());
            }
        }
    } // namespace

    ports::FilterPresetSnapshot JsonFilterPresetStore::load(std::filesystem::path path,
                                                            const std::stop_token stopToken) const {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(
            json_persistence::readBounded(path, "filter preset", stopToken), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw std::runtime_error("invalid filter preset json: " + qt::toUtf8(path) + ": " +
                                     parseError.errorString().toStdString());
        }
        if (!document.isObject()) {
            throw std::runtime_error("invalid filter preset json object: " + qt::toUtf8(path));
        }
        return FilterPresetJsonCodec{}.snapshotFromDocument(document);
    }

    void JsonFilterPresetStore::save(std::filesystem::path path,
                                     const ports::FilterPresetSnapshot& snapshot,
                                     const std::stop_token stopToken) const {
        json_persistence::throwIfCanceled(stopToken);
        ensureParentDirectory(path);

        const auto payload =
            FilterPresetJsonCodec{}.documentFromSnapshot(snapshot).toJson(QJsonDocument::Compact);
        json_persistence::writeAtomically(path, payload, "filter preset", stopToken);
    }

} // namespace ssa::infra::preferences
