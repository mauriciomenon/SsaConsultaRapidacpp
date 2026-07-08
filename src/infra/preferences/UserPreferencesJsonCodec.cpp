#include "infra/preferences/UserPreferencesJsonCodec.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "infra/preferences/FilterPreferencesJsonCodec.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>

namespace ssa::infra::preferences {

    namespace {
        constexpr int kCurrentPreferencesSchemaVersion = 7;
        constexpr std::string_view kExecutorColumnKey = "setor_executor";
        constexpr std::string_view kDerivedCountColumnKey = "qtd_derivadas";
        constexpr std::array<std::string_view, 3> kLegacyHiddenVisibleColumns{
            "equipamento", "grau_prioridade_emissao", "grau_prioridade_planejamento"};
        constexpr std::array<std::string_view, 1> kLegacyVisibleColumnsToDrop{
            "descricao_localizacao"};
        struct WidthMigration {
            std::string_view key;
            int oldWidth{0};
            int newWidth{0};
        };
        constexpr std::array<WidthMigration, 8> kSchema5WidthMigrations{{
            {"localizacao_codigo", 120, 104},
            {"setor_emissor", 90, 82},
            {"setor_executor", 90, 84},
            {"data_cadastro", 120, 112},
            {"semana_cadastro", 95, 90},
            {"descricao_ssa", 360, 560},
            {"solicitante", 180, 220},
            {"responsavel_programacao", 180, 240},
        }};
        constexpr std::array<WidthMigration, 15> kSchema6WidthMigrations{{
            {"numero_ssa", 110, 98},
            {"situacao", 80, 60},
            {"localizacao_codigo", 104, 84},
            {"setor_emissor", 82, 72},
            {"setor_executor", 84, 72},
            {"qtd_derivadas", 72, 70},
            {"derivada_de", 82, 78},
            {"data_cadastro", 112, 100},
            {"semana_cadastro", 90, 84},
            {"descricao_ssa", 560, 640},
            {"solicitante", 220, 240},
            {"responsavel_programacao", 240, 250},
            {"responsavel_execucao", 240, 250},
            {"semana_programada", 95, 86},
            {"semana_executada", 95, 86},
        }};
        constexpr std::array<WidthMigration, 1> kSchema7WidthMigrations{{
            {"derivada_de", 96, 78},
        }};

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
                    if (domain::ColumnCatalog::contains(key) &&
                        std::ranges::find(kLegacyHiddenVisibleColumns, key) ==
                            kLegacyHiddenVisibleColumns.end() &&
                        std::ranges::find(kLegacyVisibleColumnsToDrop, key) ==
                            kLegacyVisibleColumnsToDrop.end()) {
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

        bool hasManualFilterState(const ports::FilterPreferencesSnapshot& filters) {
            return !filters.searchText.empty() || !filters.columnFilters.empty() ||
                   !filters.advancedTextFilters.empty() || !filters.advancedYear.empty() ||
                   !filters.advancedWeek.empty() || !filters.issueYear.empty() ||
                   !filters.executionYear.empty() || !filters.reprogrammingEquals.empty() ||
                   !filters.reprogrammingValues.empty() || !filters.issueWeekStart.empty() ||
                   !filters.issueWeekEnd.empty() || !filters.executionWeekStart.empty() ||
                   !filters.executionWeekEnd.empty() || filters.derivationMode != "all" ||
                   filters.onlyReprogrammed;
        }

        void migrateDefaultQuickSector(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 6 || !snapshot.filters.quickSector.empty() ||
                hasManualFilterState(snapshot.filters)) {
                return;
            }
            snapshot.filters.quickSector = "IEE3";
        }

        void migrateDefaultColumnWidths(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 5) {
                return;
            }
            for (const auto& migration : kSchema5WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
            if (const auto width = snapshot.columnWidths.find("responsavel_execucao");
                width != snapshot.columnWidths.end() && width->second == 180) {
                width->second = 240;
            }
        }

        void migrateSchema6ColumnWidths(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 6) {
                return;
            }
            for (const auto& migration : kSchema6WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
        }

        void migrateSchema7ColumnWidths(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= kCurrentPreferencesSchemaVersion) {
                return;
            }
            for (const auto& migration : kSchema7WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
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

        std::vector<ports::SavedFilterSnapshot> readSavedFilters(const QJsonObject& root) {
            std::vector<ports::SavedFilterSnapshot> filters;
            const QJsonArray savedFilters = root.value("saved_filters").toArray();
            filters.reserve(static_cast<std::size_t>(savedFilters.size()));
            for (const auto& value : savedFilters) {
                if (!value.isObject()) {
                    continue;
                }
                const auto object = value.toObject();
                auto name = object.value("name").toString().trimmed().toStdString();
                if (name.empty()) {
                    continue;
                }
                ports::SavedFilterSnapshot saved;
                saved.name = std::move(name);
                saved.filters = FilterPreferencesJsonCodec{}.filtersFromObject(
                    object.value("filters").toObject(), saved.filters);
                filters.push_back(std::move(saved));
            }
            std::ranges::sort(filters, [](const auto& lhs, const auto& rhs) {
                return QString::fromStdString(lhs.name).localeAwareCompare(
                           QString::fromStdString(rhs.name)) < 0;
            });
            return filters;
        }

        QJsonArray savedFiltersToJson(const std::vector<ports::SavedFilterSnapshot>& filters) {
            QJsonArray savedFilters;
            for (const auto& filter : filters) {
                if (filter.name.empty()) {
                    continue;
                }
                QJsonObject item;
                QJsonObject filterObject;
                item.insert("name", QString::fromStdString(filter.name));
                FilterPreferencesJsonCodec{}.writeFilters(filterObject, filter.filters);
                item.insert("filters", filterObject);
                savedFilters.append(item);
            }
            return savedFilters;
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
            root.insert("saved_filters", savedFiltersToJson(snapshot.savedFilters));
        }

    } // namespace

    ports::UserPreferencesSnapshot
    UserPreferencesJsonCodec::snapshotFromDocument(const QJsonDocument& document) {
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
        snapshot.columnWidths = readColumnWidths(root);
        snapshot.savedFilters = readSavedFilters(root);
        migrateDefaultColumnWidths(snapshot);
        migrateSchema6ColumnWidths(snapshot);
        migrateSchema7ColumnWidths(snapshot);
        snapshot.schemaVersion = std::max(snapshot.schemaVersion, kCurrentPreferencesSchemaVersion);
        return snapshot;
    }

    QJsonDocument
    UserPreferencesJsonCodec::documentFromSnapshot(const ports::UserPreferencesSnapshot& snapshot) {
        QJsonObject root;
        writeWindowPreferences(root, snapshot);
        writeColumnPreferences(root, snapshot);
        FilterPreferencesJsonCodec{}.writeFilters(root, snapshot.filters);
        return QJsonDocument(root);
    }

} // namespace ssa::infra::preferences
