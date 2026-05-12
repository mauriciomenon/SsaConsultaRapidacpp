#include "infra/preferences/JsonUserPreferencesStore.h"

#include "domain/ColumnCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>
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
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            throw std::runtime_error("invalid preferences json: " + path_.string());
        }

        const QJsonObject root = document.object();
        snapshot.schemaVersion = root.value("schema_version").toInt(snapshot.schemaVersion);
        snapshot.pageSize = root.value("page_size").toInt(snapshot.pageSize);
        snapshot.theme =
            root.value("theme").toString(QString::fromStdString(snapshot.theme)).toStdString();
        snapshot.density =
            root.value("density").toString(QString::fromStdString(snapshot.density)).toStdString();
        snapshot.detailsVisible = root.value("details_visible").toBool(snapshot.detailsVisible);
        snapshot.quickSector = root.value("quick_sector")
                                   .toString(QString::fromStdString(snapshot.quickSector))
                                   .toStdString();
        snapshot.excludeScaSesSte =
            root.value("exclude_sca_ses_ste").toBool(snapshot.excludeScaSesSte);

        const QJsonArray visibleColumns = root.value("visible_columns").toArray();
        if (!visibleColumns.isEmpty()) {
            std::vector<std::string> parsedColumns;
            for (const auto& value : visibleColumns) {
                if (value.isString()) {
                    parsedColumns.push_back(value.toString().toStdString());
                }
            }
            if (!parsedColumns.empty()) {
                snapshot.visibleColumns = std::move(parsedColumns);
            }
        }

        const QJsonObject columnWidths = root.value("column_widths").toObject();
        for (auto iterator = columnWidths.begin(); iterator != columnWidths.end(); ++iterator) {
            snapshot.columnWidths[iterator.key().toStdString()] = iterator.value().toInt();
        }

        const QJsonObject columnFilters = root.value("column_filters").toObject();
        for (auto iterator = columnFilters.begin(); iterator != columnFilters.end(); ++iterator) {
            snapshot.columnFilters[iterator.key().toStdString()] =
                iterator.value().toString().toStdString();
        }

        return snapshot;
    }

    void JsonUserPreferencesStore::save(const ports::UserPreferencesSnapshot& snapshot) const {
        std::filesystem::create_directories(path_.parent_path());

        QJsonArray visibleColumns;
        for (const std::string& key : snapshot.visibleColumns) {
            visibleColumns.append(QString::fromStdString(key));
        }

        QJsonObject columnWidths;
        for (const auto& [key, width] : snapshot.columnWidths) {
            columnWidths.insert(QString::fromStdString(key), width);
        }

        QJsonObject columnFilters;
        for (const auto& [key, value] : snapshot.columnFilters) {
            columnFilters.insert(QString::fromStdString(key), QString::fromStdString(value));
        }

        QJsonObject root;
        root.insert("schema_version", snapshot.schemaVersion);
        root.insert("page_size", snapshot.pageSize);
        root.insert("theme", QString::fromStdString(snapshot.theme));
        root.insert("density", QString::fromStdString(snapshot.density));
        root.insert("details_visible", snapshot.detailsVisible);
        root.insert("visible_columns", visibleColumns);
        root.insert("column_widths", columnWidths);
        root.insert("quick_sector", QString::fromStdString(snapshot.quickSector));
        root.insert("exclude_sca_ses_ste", snapshot.excludeScaSesSte);
        root.insert("column_filters", columnFilters);

        QFile output(QString::fromStdString(path_.string()));
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("cannot write preferences file: " + path_.string());
        }
        output.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

} // namespace ssa::infra::preferences
