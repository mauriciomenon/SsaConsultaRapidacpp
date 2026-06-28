#include "infra/preferences/JsonUserPreferencesStore.h"

#include "domain/ColumnCatalog.h"
#include "infra/preferences/UserPreferencesJsonCodec.h"

#include <QFile>
#include <QJsonDocument>

#include <stdexcept>
#include <system_error>
#include <utility>

namespace ssa::infra::preferences {

    JsonUserPreferencesStore::JsonUserPreferencesStore(std::filesystem::path path)
        : path_(std::move(path)) {}

    ports::UserPreferencesSnapshot JsonUserPreferencesStore::load() const {
        ports::UserPreferencesSnapshot snapshot;
        snapshot.visibleColumns = domain::ColumnCatalog::defaultVisibleKeys();

        if (!std::filesystem::exists(path_)) {
            return snapshot;
        }

        QFile input(QString::fromStdString(path_.string()));
        if (!input.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("cannot read preferences file: " + path_.string());
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw std::runtime_error("invalid preferences json: " + path_.string());
        }
        return UserPreferencesJsonCodec{}.snapshotFromDocument(document);
    }

    void JsonUserPreferencesStore::save(const ports::UserPreferencesSnapshot& snapshot) const {
        std::error_code error;
        std::filesystem::create_directories(path_.parent_path(), error);
        if (error) {
            throw std::runtime_error("cannot create preferences directory: " + error.message());
        }

        QFile output(QString::fromStdString(path_.string()));
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("cannot write preferences file: " + path_.string());
        }

        const auto document = UserPreferencesJsonCodec{}.documentFromSnapshot(snapshot);
        if (output.write(document.toJson(QJsonDocument::Compact)) < 0) {
            throw std::runtime_error("cannot write preferences file: " +
                                     output.errorString().toStdString());
        }
    }

} // namespace ssa::infra::preferences
