#include "infra/preferences/UserPreferencesJsonCodec.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "infra/preferences/FilterPreferencesJsonCodec.h"
#include "infra/preferences/JsonPersistenceSupport.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <stdexcept>

namespace ssa::infra::preferences {

    namespace {
        constexpr std::string_view kDerivationColumnKey = "derivada_de";
        constexpr int kDerivationColumnWidth = 88;
        constexpr std::string_view kExecutorColumnKey = "setor_executor";
        constexpr std::string_view kDerivedCountColumnKey = "qtd_derivadas";
        constexpr std::array<std::string_view, 16> kSchema8DefaultVisibleColumns{{
            "numero_ssa",
            "situacao",
            "derivada_de",
            "localizacao_codigo",
            "semana_cadastro",
            "data_cadastro",
            "descricao_ssa",
            "descricao_execucao",
            "setor_emissor",
            "setor_executor",
            "qtd_derivadas",
            "solicitante",
            "responsavel_programacao",
            "responsavel_execucao",
            "semana_programada",
            "semana_executada",
        }};
        constexpr std::array<std::string_view, 16> kSchema10DefaultVisibleColumns{{
            "numero_ssa",
            "situacao",
            "localizacao_codigo",
            "setor_emissor",
            "setor_executor",
            "qtd_derivadas",
            "descricao_ssa",
            "data_cadastro",
            "derivada_de",
            "semana_cadastro",
            "solicitante",
            "responsavel_programacao",
            "responsavel_execucao",
            "semana_programada",
            "semana_executada",
            "descricao_execucao",
        }};
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
            {"derivada_de", 82, kDerivationColumnWidth},
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
            {"derivada_de", 96, kDerivationColumnWidth},
        }};
        constexpr std::array<WidthMigration, 4> kSchema10WidthMigrations{{
            {"setor_emissor", 72, 68},
            {"setor_executor", 72, 68},
            {"qtd_derivadas", 70, 66},
            {"derivada_de", 74, 62},
        }};
        constexpr std::array<WidthMigration, 1> kSchema11WidthMigrations{{
            {"derivada_de", 62, kDerivationColumnWidth},
        }};

        bool matchesVisibleColumns(const std::vector<std::string>& columns,
                                   const std::span<const std::string_view> expected) {
            if (columns.size() != expected.size()) {
                return false;
            }
            for (std::size_t index = 0; index < expected.size(); ++index) {
                if (columns[index] != expected[index]) {
                    return false;
                }
            }
            return true;
        }

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
                        if (std::ranges::find(parsedColumns, key) != parsedColumns.end()) {
                            throw std::runtime_error("duplicate visible column: " + key);
                        }
                        parsedColumns.push_back(key);
                    }
                }
            }
            return parsedColumns.empty() ? std::move(defaults) : std::move(parsedColumns);
        }

        void migrateDerivedCountColumn(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= ports::kCurrentUserPreferencesSchemaVersion) {
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
                   !filters.executionYear.empty() || !filters.reprogrammingValues.empty() ||
                   !filters.issueWeekStart.empty() || !filters.issueWeekEnd.empty() ||
                   !filters.executionWeekStart.empty() || !filters.executionWeekEnd.empty() ||
                   filters.derivationMode != "all" || filters.onlyReprogrammed;
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
            if (snapshot.schemaVersion >= 7) {
                return;
            }
            for (const auto& migration : kSchema7WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
        }

        void migrateSchema8DerivationWidth(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= ports::kCurrentUserPreferencesSchemaVersion) {
                return;
            }
            const auto width = snapshot.columnWidths.find(std::string{kDerivationColumnKey});
            if (width != snapshot.columnWidths.end() && width->second > kDerivationColumnWidth) {
                width->second = kDerivationColumnWidth;
            }
        }

        void migrateSchema9DefaultColumns(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 9) {
                return;
            }
            if (matchesVisibleColumns(snapshot.visibleColumns, kSchema8DefaultVisibleColumns)) {
                snapshot.visibleColumns = domain::ColumnCatalog::defaultVisibleKeys();
            }
        }

        void migrateSchema10CompactColumns(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 10) {
                return;
            }
            for (const auto& migration : kSchema10WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
        }

        void migrateSchema11DefaultColumns(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= 11) {
                return;
            }
            if (matchesVisibleColumns(snapshot.visibleColumns, kSchema10DefaultVisibleColumns)) {
                snapshot.visibleColumns = domain::ColumnCatalog::defaultVisibleKeys();
            }
        }

        void migrateSchema11DerivationWidth(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= ports::kCurrentUserPreferencesSchemaVersion) {
                return;
            }
            for (const auto& migration : kSchema11WidthMigrations) {
                const auto width = snapshot.columnWidths.find(std::string{migration.key});
                if (width != snapshot.columnWidths.end() && width->second == migration.oldWidth) {
                    width->second = migration.newWidth;
                }
            }
        }

        void removeVisibleColumn(std::vector<std::string>& columns, const std::string_view key) {
            const auto first = std::ranges::remove(columns, std::string{key}).begin();
            columns.erase(first, columns.end());
        }

        void moveColumnAfter(std::vector<std::string>& columns, const std::string_view column,
                             const std::string_view previous) {
            auto columnIt = std::ranges::find(columns, std::string{column});
            const auto previousIt = std::ranges::find(columns, std::string{previous});
            if (columnIt == columns.end() || previousIt == columns.end() ||
                std::next(previousIt) == columnIt) {
                return;
            }
            std::string value = std::move(*columnIt);
            columns.erase(columnIt);
            const auto refreshedPrevious = std::ranges::find(columns, std::string{previous});
            columns.insert(std::next(refreshedPrevious), std::move(value));
        }

        void migrateSchema12VisibleColumns(ports::UserPreferencesSnapshot& snapshot) {
            if (snapshot.schemaVersion >= ports::kCurrentUserPreferencesSchemaVersion) {
                return;
            }
            removeVisibleColumn(snapshot.visibleColumns, "data_cadastro");
            moveColumnAfter(snapshot.visibleColumns, "semana_cadastro", "descricao_ssa");
            moveColumnAfter(snapshot.visibleColumns, "solicitante", "semana_cadastro");
            moveColumnAfter(snapshot.visibleColumns, "derivada_de", "solicitante");
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
            if (savedFilters.size() > static_cast<qsizetype>(ports::kMaxSavedFilterCount)) {
                throw std::runtime_error("too many saved filters");
            }
            filters.reserve(static_cast<std::size_t>(savedFilters.size()));
            for (const auto& value : savedFilters) {
                if (!value.isObject()) {
                    continue;
                }
                const auto object = value.toObject();
                const auto jsonName = object.value("name").toString().trimmed();
                if (jsonName.isEmpty()) {
                    continue;
                }
                if (jsonName.size() > static_cast<qsizetype>(ports::kMaxSavedFilterNameLength)) {
                    throw std::runtime_error("saved filter name too long");
                }
                ports::SavedFilterSnapshot saved;
                saved.name = jsonName.toStdString();
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
            if (filters.size() > ports::kMaxSavedFilterCount) {
                throw std::runtime_error("too many saved filters");
            }
            QJsonArray savedFilters;
            for (const auto& filter : filters) {
                if (filter.name.empty()) {
                    continue;
                }
                const auto name = QString::fromStdString(filter.name);
                if (name.size() > static_cast<qsizetype>(ports::kMaxSavedFilterNameLength)) {
                    throw std::runtime_error("saved filter name too long");
                }
                QJsonObject item;
                QJsonObject filterObject;
                item.insert("name", name);
                FilterPreferencesJsonCodec{}.writeFilters(filterObject, filter.filters);
                item.insert("filters", filterObject);
                savedFilters.append(item);
            }
            return savedFilters;
        }

        ports::SamRefreshPreferencesSnapshot readSamRefreshPreferences(const QJsonObject& root) {
            ports::SamRefreshPreferencesSnapshot preferences;
            const auto object = root.value("sam_refresh").toObject();
            preferences.scrapReportRoot =
                object.value("scrap_report_root").toString().toStdString();
            preferences.caFile = object.value("ca_file").toString().toStdString();
            preferences.baseUrl = object.value("base_url")
                                      .toString(QString::fromStdString(preferences.baseUrl))
                                      .toStdString();
            preferences.executorSectors = object.value("executor_sectors").toString().toStdString();
            preferences.scope = object.value("scope")
                                    .toString(QString::fromStdString(preferences.scope))
                                    .toStdString();
            preferences.intervalMinutes = std::clamp(
                object.value("interval_minutes").toInt(preferences.intervalMinutes), 1, 30'000);
            preferences.enabled = object.value("enabled").toBool(false);
            preferences.autoRefreshEnabled = object.value("auto_refresh_enabled").toBool(false);
            return preferences;
        }

        QJsonObject
        samRefreshPreferencesToJson(const ports::SamRefreshPreferencesSnapshot& preferences) {
            return {{"scrap_report_root", QString::fromStdString(preferences.scrapReportRoot)},
                    {"ca_file", QString::fromStdString(preferences.caFile)},
                    {"base_url", QString::fromStdString(preferences.baseUrl)},
                    {"executor_sectors", QString::fromStdString(preferences.executorSectors)},
                    {"scope", QString::fromStdString(preferences.scope)},
                    {"interval_minutes", std::clamp(preferences.intervalMinutes, 1, 30'000)},
                    {"enabled", preferences.enabled},
                    {"auto_refresh_enabled", preferences.autoRefreshEnabled}};
        }

        void writeWindowPreferences(QJsonObject& root,
                                    const ports::UserPreferencesSnapshot& snapshot) {
            root.insert("schema_version", ports::kCurrentUserPreferencesSchemaVersion);
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

        snapshot.schemaVersion = json_persistence::schemaVersion(
            root, ports::kCurrentUserPreferencesSchemaVersion, "preferences file");
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
        snapshot.samRefresh = readSamRefreshPreferences(root);
        migrateDefaultColumnWidths(snapshot);
        migrateSchema6ColumnWidths(snapshot);
        migrateSchema7ColumnWidths(snapshot);
        migrateSchema8DerivationWidth(snapshot);
        migrateSchema9DefaultColumns(snapshot);
        migrateSchema10CompactColumns(snapshot);
        migrateSchema11DefaultColumns(snapshot);
        migrateSchema11DerivationWidth(snapshot);
        migrateSchema12VisibleColumns(snapshot);
        snapshot.schemaVersion = ports::kCurrentUserPreferencesSchemaVersion;
        return snapshot;
    }

    QJsonDocument
    UserPreferencesJsonCodec::documentFromSnapshot(const ports::UserPreferencesSnapshot& snapshot) {
        QJsonObject root;
        writeWindowPreferences(root, snapshot);
        writeColumnPreferences(root, snapshot);
        root.insert("sam_refresh", samRefreshPreferencesToJson(snapshot.samRefresh));
        FilterPreferencesJsonCodec{}.writeFilters(root, snapshot.filters);
        return QJsonDocument(root);
    }

} // namespace ssa::infra::preferences
