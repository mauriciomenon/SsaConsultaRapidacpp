#include "infra/preferences/FilterPreferencesJsonCodec.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaDerivationMode.h"

#include <array>
#include <span>

namespace ssa::infra::preferences {

    namespace {
        struct StringFilterBinding {
            const char* key;
            std::string ports::FilterPreferencesSnapshot::* field;
        };

        constexpr std::array<StringFilterBinding, 9> kStringFilters{{
            {"advanced_year", &ports::FilterPreferencesSnapshot::advancedYear},
            {"advanced_week", &ports::FilterPreferencesSnapshot::advancedWeek},
            {"issue_year", &ports::FilterPreferencesSnapshot::issueYear},
            {"execution_year", &ports::FilterPreferencesSnapshot::executionYear},
            {"reprogramming_equals", &ports::FilterPreferencesSnapshot::reprogrammingEquals},
            {"issue_week_start", &ports::FilterPreferencesSnapshot::issueWeekStart},
            {"issue_week_end", &ports::FilterPreferencesSnapshot::issueWeekEnd},
            {"execution_week_start", &ports::FilterPreferencesSnapshot::executionWeekStart},
            {"execution_week_end", &ports::FilterPreferencesSnapshot::executionWeekEnd},
        }};

        std::map<std::string, std::string> readFilterMap(const QJsonObject& root,
                                                         const char* keyName) {
            std::map<std::string, std::string> filters;
            const QJsonObject values = root.value(keyName).toObject();
            for (auto iterator = values.begin(); iterator != values.end(); ++iterator) {
                const auto key = iterator.key().toStdString();
                if (domain::ColumnCatalog::contains(key)) {
                    filters[key] = iterator.value().toString().toStdString();
                }
            }
            return filters;
        }

        QJsonObject filtersToJson(const std::map<std::string, std::string>& filters) {
            QJsonObject values;
            for (const auto& [key, value] : filters) {
                if (domain::ColumnCatalog::contains(key)) {
                    values.insert(QString::fromStdString(key), QString::fromStdString(value));
                }
            }
            return values;
        }

        std::string jsonString(const QJsonObject& root, const char* key,
                               const std::string& defaultValue) {
            return root.value(key).toString(QString::fromStdString(defaultValue)).toStdString();
        }

        void readStringFilters(const QJsonObject& root, ports::FilterPreferencesSnapshot& filters,
                               const std::span<const StringFilterBinding> bindings) {
            for (const auto& binding : bindings) {
                auto& field = filters.*(binding.field);
                field =
                    root.value(binding.key).toString(QString::fromStdString(field)).toStdString();
            }
        }

        void writeStringFilters(QJsonObject& root, const ports::FilterPreferencesSnapshot& filters,
                                const std::span<const StringFilterBinding> bindings) {
            for (const auto& binding : bindings) {
                root.insert(binding.key, QString::fromStdString(filters.*(binding.field)));
            }
        }
    } // namespace

    ports::FilterPreferencesSnapshot FilterPreferencesJsonCodec::filtersFromObject(
        const QJsonObject& root, ports::FilterPreferencesSnapshot baseSnapshot) const {
        baseSnapshot.searchText = jsonString(root, "search_text", baseSnapshot.searchText);
        baseSnapshot.quickSector = jsonString(root, "quick_sector", baseSnapshot.quickSector);
        baseSnapshot.excludeScaSesSte =
            root.value("exclude_sca_ses_ste").toBool(baseSnapshot.excludeScaSesSte);
        baseSnapshot.columnFilters = readFilterMap(root, "column_filters");
        baseSnapshot.advancedTextFilters = readFilterMap(root, "advanced_text_filters");
        baseSnapshot.advancedWeekColumnKey =
            jsonString(root, "advanced_week_column_key", baseSnapshot.advancedWeekColumnKey);
        if (!domain::ColumnCatalog::contains(baseSnapshot.advancedWeekColumnKey)) {
            baseSnapshot.advancedWeekColumnKey =
                std::string{domain::ColumnCatalog::defaultAdvancedWeekColumnKey()};
        }
        readStringFilters(root, baseSnapshot, kStringFilters);
        baseSnapshot.reprogrammingMode = domain::normalizedNumericComparisonMode(
            jsonString(root, "reprogramming_mode", baseSnapshot.reprogrammingMode));
        baseSnapshot.derivationMode = domain::normalizedDerivationMode(
            jsonString(root, "derivation_mode", baseSnapshot.derivationMode));
        baseSnapshot.onlyReprogrammed =
            root.value("only_reprogrammed").toBool(baseSnapshot.onlyReprogrammed);
        return baseSnapshot;
    }

    void FilterPreferencesJsonCodec::writeFilters(
        QJsonObject& root, const ports::FilterPreferencesSnapshot& filters) const {
        root.insert("search_text", QString::fromStdString(filters.searchText));
        root.insert("quick_sector", QString::fromStdString(filters.quickSector));
        root.insert("exclude_sca_ses_ste", filters.excludeScaSesSte);
        root.insert("column_filters", filtersToJson(filters.columnFilters));
        root.insert("advanced_text_filters", filtersToJson(filters.advancedTextFilters));
        const auto advancedWeekColumnKey =
            domain::ColumnCatalog::contains(filters.advancedWeekColumnKey)
                ? filters.advancedWeekColumnKey
                : std::string{domain::ColumnCatalog::defaultAdvancedWeekColumnKey()};
        root.insert("advanced_week_column_key", QString::fromStdString(advancedWeekColumnKey));
        writeStringFilters(root, filters, kStringFilters);
        root.insert("reprogramming_mode",
                    QString::fromStdString(
                        domain::normalizedNumericComparisonMode(filters.reprogrammingMode)));
        root.insert("derivation_mode", QString::fromStdString(domain::normalizedDerivationMode(
                                           filters.derivationMode)));
        root.insert("only_reprogrammed", filters.onlyReprogrammed);
    }

} // namespace ssa::infra::preferences
