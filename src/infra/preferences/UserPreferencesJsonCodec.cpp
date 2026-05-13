#include "infra/preferences/UserPreferencesJsonCodec.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <QJsonArray>
#include <QJsonObject>

#include <stdexcept>

namespace ssa::infra::preferences {

    namespace {

        std::vector<std::string> readVisibleColumns(const QJsonObject& root,
                                                    std::vector<std::string> defaults) {
            const QJsonArray visibleColumns = root.value("visible_columns").toArray();
            if (visibleColumns.isEmpty()) {
                return defaults;
            }
            std::vector<std::string> parsedColumns;
            for (const auto& value : visibleColumns) {
                if (value.isString()) {
                    const auto key = value.toString().toStdString();
                    if (domain::ColumnCatalog::contains(key)) {
                        parsedColumns.push_back(key);
                    }
                }
            }
            return parsedColumns.empty() ? std::move(defaults) : std::move(parsedColumns);
        }

        std::map<std::string, int> readColumnWidths(const QJsonObject& root) {
            std::map<std::string, int> widths;
            const QJsonObject columnWidths = root.value("column_widths").toObject();
            for (auto iterator = columnWidths.begin(); iterator != columnWidths.end(); ++iterator) {
                widths[iterator.key().toStdString()] = iterator.value().toInt();
            }
            return widths;
        }

        std::map<std::string, std::string> readColumnFilters(const QJsonObject& root) {
            std::map<std::string, std::string> filters;
            const QJsonObject columnFilters = root.value("column_filters").toObject();
            for (auto iterator = columnFilters.begin(); iterator != columnFilters.end();
                 ++iterator) {
                const auto key = iterator.key().toStdString();
                if (domain::ColumnCatalog::contains(key)) {
                    filters[key] = iterator.value().toString().toStdString();
                }
            }
            return filters;
        }

        void populateSortPreferences(const QJsonObject& root,
                                     ports::UserPreferencesSnapshot& snapshot) {
            const auto defaultSortColumnKey = snapshot.sortColumnKey;
            snapshot.sortColumnKey = root.value("sort_column_key")
                                         .toString(QString::fromStdString(defaultSortColumnKey))
                                         .toStdString();
            if (!domain::ColumnCatalog::contains(snapshot.sortColumnKey)) {
                snapshot.sortColumnKey = defaultSortColumnKey;
            }
            snapshot.sortAscending = root.value("sort_ascending").toBool(snapshot.sortAscending);
        }

        void populateAdvancedPreferences(const QJsonObject& root,
                                         ports::UserPreferencesSnapshot& snapshot) {
            const auto defaultAdvancedWeekColumnKey = snapshot.advancedWeekColumnKey;
            snapshot.advancedWeekColumnKey =
                root.value("advanced_week_column_key")
                    .toString(QString::fromStdString(defaultAdvancedWeekColumnKey))
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
        }

        QJsonArray visibleColumnsToJson(const std::vector<std::string>& columns) {
            QJsonArray visibleColumns;
            for (const std::string& key : columns) {
                visibleColumns.append(QString::fromStdString(key));
            }
            return visibleColumns;
        }

        QJsonObject columnWidthsToJson(const std::map<std::string, int>& widths) {
            QJsonObject columnWidths;
            for (const auto& [key, width] : widths) {
                columnWidths.insert(QString::fromStdString(key), width);
            }
            return columnWidths;
        }

        QJsonObject columnFiltersToJson(const std::map<std::string, std::string>& filters) {
            QJsonObject columnFilters;
            for (const auto& [key, value] : filters) {
                columnFilters.insert(QString::fromStdString(key), QString::fromStdString(value));
            }
            return columnFilters;
        }

    } // namespace

    ports::UserPreferencesSnapshot
    UserPreferencesJsonCodec::snapshotFromDocument(const QJsonDocument& document) const {
        if (!document.isObject()) {
            throw std::runtime_error("invalid preferences json");
        }

        ports::UserPreferencesSnapshot snapshot;
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
        populateSortPreferences(root, snapshot);
        snapshot.quickSector = root.value("quick_sector")
                                   .toString(QString::fromStdString(snapshot.quickSector))
                                   .toStdString();
        snapshot.excludeScaSesSte =
            root.value("exclude_sca_ses_ste").toBool(snapshot.excludeScaSesSte);
        populateAdvancedPreferences(root, snapshot);
        snapshot.visibleColumns =
            readVisibleColumns(root, domain::ColumnCatalog::defaultVisibleKeys());
        snapshot.columnWidths = readColumnWidths(root);
        snapshot.columnFilters = readColumnFilters(root);
        return snapshot;
    }

    QJsonDocument UserPreferencesJsonCodec::documentFromSnapshot(
        const ports::UserPreferencesSnapshot& snapshot) const {
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
        root.insert("visible_columns", visibleColumnsToJson(snapshot.visibleColumns));
        root.insert("column_widths", columnWidthsToJson(snapshot.columnWidths));
        root.insert("quick_sector", QString::fromStdString(snapshot.quickSector));
        root.insert("exclude_sca_ses_ste", snapshot.excludeScaSesSte);
        root.insert("column_filters", columnFiltersToJson(snapshot.columnFilters));
        root.insert("advanced_week_column_key",
                    QString::fromStdString(snapshot.advancedWeekColumnKey));
        root.insert("advanced_year", QString::fromStdString(snapshot.advancedYear));
        root.insert("advanced_week", QString::fromStdString(snapshot.advancedWeek));
        root.insert("derivation_mode", QString::fromStdString(snapshot.derivationMode));
        root.insert("only_reprogrammed", snapshot.onlyReprogrammed);
        return QJsonDocument(root);
    }

} // namespace ssa::infra::preferences
