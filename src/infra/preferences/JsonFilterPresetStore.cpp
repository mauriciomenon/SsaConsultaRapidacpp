#include "infra/preferences/JsonFilterPresetStore.h"

#include "infra/preferences/FilterPresetJsonCodec.h"

#include <QFile>
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

    ports::FilterPresetSnapshot JsonFilterPresetStore::load(std::filesystem::path path) const {
        QFile input(QString::fromStdString(path.string()));
        if (!input.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("cannot read filter preset: " + path.string());
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw std::runtime_error("invalid filter preset json: " + path.string() + ": " +
                                     parseError.errorString().toStdString());
        }
        if (!document.isObject()) {
            throw std::runtime_error("invalid filter preset json object: " + path.string());
        }
        return FilterPresetJsonCodec{}.snapshotFromDocument(document);
    }

    void JsonFilterPresetStore::save(std::filesystem::path path,
                                     const ports::FilterPresetSnapshot& snapshot) const {
        ensureParentDirectory(path);

        QFile output(QString::fromStdString(path.string()));
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("cannot write filter preset: " + path.string());
        }
        output.write(
            FilterPresetJsonCodec{}.documentFromSnapshot(snapshot).toJson(QJsonDocument::Compact));
    }

} // namespace ssa::infra::preferences
