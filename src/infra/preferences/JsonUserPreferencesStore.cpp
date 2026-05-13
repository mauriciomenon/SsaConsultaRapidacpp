#include "infra/preferences/JsonUserPreferencesStore.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

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
        snapshot.pageSize = domain::clampPageSize(root.value("page_size").toInt(snapshot.pageSize));
        snapshot.theme =
            root.value("theme").toString(QString::fromStdString(snapshot.theme)).toStdString();
        snapshot.density =
            root.value("density").toString(QString::fromStdString(snapshot.density)).toStdString();
        snapshot.detailsVisible = root.value("details_visible").toBool(snapshot.detailsVisible);
        snapshot.detailsPanelWidth = ports::clampDetailsPanelWidth(
            root.value("details_panel_width").toInt(snapshot.detailsPanelWidth));
        const auto defaultSortColumnKey = snapshot.sortColumnKey;
        snapshot.sortColumnKey = root.value("sort_column_key")
                                     .toString(QString::fromStdString(defaultSortColumnKey))
                                     .toStdString();
        if (!domain::ColumnCatalog::contains(snapshot.sortColumnKey)) {
            snapshot.sortColumnKey = defaultSortColumnKey;
        }
        snapshot.sortAscending = root.value("sort_ascending").toBool(snapshot.sortAscending);
        snapshot.quickSector = root.value("quick_sector")
                                   .toString(QString::fromStdString(snapshot.quickSector))
                                   .toStdString();
        snapshot.excludeScaSesSte =
            root.value("exclude_sca_ses_ste").toBool(snapshot.excludeScaSesSte);
        const auto defaultAdvancedWeekColumnKey = snapshot.advancedWeekColumnKey;
        snapshot.advancedWeekColumnKey =
            root.value("advanced_week_column_key")
                .toString(QString::fromStdString(snapshot.advancedWeekColumnKey))
                .toStdString();
        if (!domain::ColumnCatalog::contains(snapshot.advancedWeekColumnKey)) {
            snapshot.advancedWeekColumnKey = defaultAdvancedWeekColumnKey;
        }
        snapshot.advancedYear = root.value("advanced_year")
                                    .toString(QString::fromStdString(snapshot.advancedYear))
                                    .toStdString();
        snapshot.advancedWeek = root.value("advanced_week")
                                    .toString(QString::fromStdString(snapshot.advancedWeek))
                                    .toStdString();
        snapshot.derivationMode = root.value("derivation_mode")
                                      .toString(QString::fromStdString(snapshot.derivationMode))
                                      .toStdString();
        snapshot.derivationMode = ports::normalizedDerivationMode(snapshot.derivationMode);
        snapshot.onlyReprogrammed =
            root.value("only_reprogrammed").toBool(snapshot.onlyReprogrammed);

        const QJsonArray visibleColumns = root.value("visible_columns").toArray();
        if (!visibleColumns.isEmpty()) {
            std::vector<std::string> parsedColumns;
            for (const auto& value : visibleColumns) {
                if (value.isString()) {
                    const auto key = value.toString().toStdString();
                    if (domain::ColumnCatalog::contains(key)) {
                        parsedColumns.push_back(key);
                    }
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
            const auto key = iterator.key().toStdString();
            if (domain::ColumnCatalog::contains(key)) {
                snapshot.columnFilters[key] = iterator.value().toString().toStdString();
            }
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
        root.insert("page_size", domain::clampPageSize(snapshot.pageSize));
        root.insert("theme", QString::fromStdString(snapshot.theme));
        root.insert("density", QString::fromStdString(snapshot.density));
        root.insert("details_visible", snapshot.detailsVisible);
        root.insert("details_panel_width",
                    ports::clampDetailsPanelWidth(snapshot.detailsPanelWidth));
        root.insert("sort_column_key", QString::fromStdString(snapshot.sortColumnKey));
        root.insert("sort_ascending", snapshot.sortAscending);
        root.insert("visible_columns", visibleColumns);
        root.insert("column_widths", columnWidths);
        root.insert("quick_sector", QString::fromStdString(snapshot.quickSector));
        root.insert("exclude_sca_ses_ste", snapshot.excludeScaSesSte);
        root.insert("column_filters", columnFilters);
        root.insert("advanced_week_column_key",
                    QString::fromStdString(snapshot.advancedWeekColumnKey));
        root.insert("advanced_year", QString::fromStdString(snapshot.advancedYear));
        root.insert("advanced_week", QString::fromStdString(snapshot.advancedWeek));
        root.insert("derivation_mode", QString::fromStdString(snapshot.derivationMode));
        root.insert("only_reprogrammed", snapshot.onlyReprogrammed);

        QFile output(QString::fromStdString(path_.string()));
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("cannot write preferences file: " + path_.string());
        }
        output.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

} // namespace ssa::infra::preferences
