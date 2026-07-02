#include "infra/preferences/UserPreferencesJsonCodec.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "infra/preferences/FilterPreferencesJsonCodec.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace ssa::infra::preferences {

    namespace {
        constexpr int kCurrentPreferencesSchemaVersion = 2;
        constexpr std::string_view kExecutorColumnKey = "setor_executor";
        constexpr std::string_view kDerivedCountColumnKey = "qtd_derivadas";

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

        void migrateDerivedCountColumn(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= kCurrentPreferencesSchemaVersion) {
                return;
            }
            auto& columns = snapshot.visibleColumns;
            if (std::ranges::find(columns, std::string{kDerivedCountColumnKey}) != columns.end()) {
                return;
            }
            const auto executor = std::ranges::find(columns, std::string{kExecutorColumnKey});
            if (executor == columns.end()) {
                return;
            }
            columns.insert(std::next(executor), std::string{kDerivedCountColumnKey});
        }

        void migrateDefaultQuickSector(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= kCurrentPreferencesSchemaVersion ||
                !snapshot.filters.quickSector.empty()) {
                return;
            }
            snapshot.filters.quickSector = ports::FilterPreferencesSnapshot{}.quickSector;
        }

        std::map<std::string, int> readColumnWidths(const QJsonObject& root) {
            std::map<std::string, int> widths;
            const QJsonObject columnWidths = root.value("column_widths").toObject();
            for (auto iterator = columnWidths.begin(); iterator != columnWidths.end(); ++iterator) {
                const auto key = iterator.key().toStdString();
                if (!domain::ColumnCatalog::contains(key)) {
                    continue;
                }
                // QJsonValue has no toInt(bool* ok); skip non-numeric widths instead of
                // silently coercing them to 0.
                const auto value = iterator.value();
                if (value.isDouble()) {
                    widths[key] = value.toInt(0);
                }
            }
            return widths;
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

        void writeWindowPreferences(QJsonObject& root,
                                    const ports::UserPreferencesSnapshot& snapshot) {
            root.insert("schema_version", snapshot.schemaVersion);
            root.insert("page_size", domain::clampPageSize(snapshot.pageSize));
            root.insert("theme", QString::fromStdString(snapshot.theme));
            root.insert("density", QString::fromStdString(snapshot.density));
            root.insert("details_visible", snapshot.detailsVisible);
            root.insert("details_panel_width",
                        ports::clampDetailsPanelWidth(snapshot.detailsPanelWidth));
        }

        void writeColumnPreferences(QJsonObject& root,
                                    const ports::UserPreferencesSnapshot& snapshot) {
            root.insert("sort_column_key", QString::fromStdString(snapshot.sortColumnKey));
            root.insert("sort_ascending", snapshot.sortAscending);
            root.insert("visible_columns", visibleColumnsToJson(snapshot.visibleColumns));
            root.insert("column_widths", columnWidthsToJson(snapshot.columnWidths));
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
        snapshot.filters = FilterPreferencesJsonCodec{}.filtersFromObject(root, snapshot.filters);
        snapshot.visibleColumns =
            readVisibleColumns(root, domain::ColumnCatalog::defaultVisibleKeys());
        migrateDerivedCountColumn(snapshot);
        migrateDefaultQuickSector(snapshot);
        snapshot.schemaVersion = std::max(snapshot.schemaVersion, kCurrentPreferencesSchemaVersion);
        snapshot.columnWidths = readColumnWidths(root);
        return snapshot;
    }

    QJsonDocument UserPreferencesJsonCodec::documentFromSnapshot(
        const ports::UserPreferencesSnapshot& snapshot) const {
        QJsonObject root;
        writeWindowPreferences(root, snapshot);
        writeColumnPreferences(root, snapshot);
        FilterPreferencesJsonCodec{}.writeFilters(root, snapshot.filters);
        return QJsonDocument(root);
    }

} // namespace ssa::infra::preferences
