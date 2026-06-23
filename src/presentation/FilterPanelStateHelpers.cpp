#include "presentation/FilterPanelStateHelpers.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace ssa::presentation::filterpanel {

    namespace {

        constexpr std::string_view kValueSeparator = ",";
        constexpr std::string_view kSpaceChars = " \t\r\n";

        [[nodiscard]] std::string_view trimCopy(std::string_view value) {
            auto begin = value.find_first_not_of(kSpaceChars);
            if (begin == std::string_view::npos) {
                return {};
            }
            const auto end = value.find_last_not_of(kSpaceChars) + 1;
            return value.substr(begin, end - begin);
        }

        void appendRangeSummary(std::vector<FilterSummaryEntry>& entries,
                                const std::string_view label, const std::string_view kind,
                                const std::optional<int> start, const std::optional<int> end) {
            if (!start.has_value() && !end.has_value()) {
                return;
            }
            std::string part{label};
            if (start.has_value() && end.has_value()) {
                part += ":";
                part += std::to_string(*start);
                part += "-";
                part += std::to_string(*end);
            } else if (start.has_value()) {
                part += ">=";
                part += std::to_string(*start);
            } else {
                part += "<=";
                part += std::to_string(*end);
            }
            entries.push_back({.text = std::move(part), .kind = std::string{kind}, .key = {}});
        }

    } // namespace

    std::optional<int> parsePositiveInt(const QString& value) {
        const auto text = value.trimmed();
        if (text.isEmpty()) {
            return std::nullopt;
        }
        bool ok = false;
        const int parsed = text.toInt(&ok);
        if (!ok || parsed <= 0) {
            return std::nullopt;
        }
        return parsed;
    }

    domain::DerivationFilterMode derivationModeFromString(const QString& value) {
        if (value == "root") {
            return domain::DerivationFilterMode::RootOnly;
        }
        if (value == "derived") {
            return domain::DerivationFilterMode::DerivedOnly;
        }
        return domain::DerivationFilterMode::All;
    }

    std::string derivationModeToString(const domain::DerivationFilterMode mode) {
        if (mode == domain::DerivationFilterMode::RootOnly) {
            return "root";
        }
        if (mode == domain::DerivationFilterMode::DerivedOnly) {
            return "derived";
        }
        return "all";
    }

    std::vector<std::string>
    buildFilterSummaryParts(const std::string_view quickSector,
                            const std::map<std::string, std::string>& columnFilters,
                            const domain::AdvancedFilterSpec& advanced) {
        const auto entries = buildFilterSummaryEntries(quickSector, columnFilters, advanced);
        std::vector<std::string> parts;
        parts.reserve(entries.size());
        std::ranges::transform(entries, std::back_inserter(parts),
                               [](const FilterSummaryEntry& entry) { return entry.text; });
        return parts;
    }

    std::vector<FilterSummaryEntry>
    buildFilterSummaryEntries(const std::string_view quickSector,
                              const std::map<std::string, std::string>& columnFilters,
                              const domain::AdvancedFilterSpec& advanced) {
        std::vector<FilterSummaryEntry> entries;
        if (!quickSector.empty()) {
            entries.push_back({.text = std::string(domain::ColumnCatalog::executorColumnKey()) +
                                       ":" + std::string(quickSector),
                               .kind = "quick_sector",
                               .key = {}});
        }
        for (const auto& [key, value] : columnFilters) {
            auto part = key;
            part += ":";
            part += value;
            entries.push_back({.text = std::move(part), .kind = "column", .key = key});
        }
        for (const auto& [key, value] : advanced.textFilters) {
            std::string part{"adv "};
            part += key;
            part += ":";
            part += value;
            entries.push_back({.text = std::move(part), .kind = "advanced_text", .key = key});
        }
        if (advanced.year.has_value()) {
            entries.push_back({.text = "ano:" + std::to_string(*advanced.year),
                               .kind = "advanced_year",
                               .key = {}});
        }
        if (advanced.week.has_value()) {
            entries.push_back({.text = "semana:" + std::to_string(*advanced.week),
                               .kind = "advanced_week",
                               .key = {}});
        }
        if (advanced.issueYear.has_value()) {
            entries.push_back({.text = "ano emissao:" + std::to_string(*advanced.issueYear),
                               .kind = "advanced_issue_year",
                               .key = {}});
        }
        if (advanced.executionYear.has_value()) {
            entries.push_back({.text = "ano execucao:" + std::to_string(*advanced.executionYear),
                               .kind = "advanced_execution_year",
                               .key = {}});
        }
        if (advanced.reprogrammingEquals.has_value()) {
            entries.push_back(
                {.text = std::string{"reprog"} +
                         domain::numericComparisonOperator(advanced.reprogrammingComparison) +
                         std::to_string(*advanced.reprogrammingEquals),
                 .kind = "advanced_reprogramming",
                 .key = {}});
        }
        appendRangeSummary(entries, "emissao", "advanced_issue_week_range", advanced.issueWeekStart,
                           advanced.issueWeekEnd);
        appendRangeSummary(entries, "execucao", "advanced_execution_week_range",
                           advanced.executionWeekStart, advanced.executionWeekEnd);
        if (advanced.derivationMode == domain::DerivationFilterMode::RootOnly) {
            entries.push_back(
                {.text = "somente originais", .kind = "advanced_derivation_mode", .key = {}});
        } else if (advanced.derivationMode == domain::DerivationFilterMode::DerivedOnly) {
            entries.push_back(
                {.text = "somente derivadas", .kind = "advanced_derivation_mode", .key = {}});
        }
        if (advanced.onlyReprogrammed) {
            entries.push_back(
                {.text = "somente reprogramadas", .kind = "advanced_only_reprogrammed", .key = {}});
        }
        return entries;
    }

    bool hasFilterValue(const std::string& currentFilter, const std::string& candidateValue) {
        const auto normalizedCandidate = trimCopy(candidateValue);
        if (normalizedCandidate.empty()) {
            return false;
        }
        if (currentFilter.empty()) {
            return false;
        }

        std::size_t index = 0;
        while (index < currentFilter.size()) {
            const auto next = currentFilter.find(kValueSeparator, index);
            const auto limit = (next == std::string::npos) ? currentFilter.size() : next;
            if (trimCopy(std::string_view{currentFilter}.substr(index, limit - index)) ==
                normalizedCandidate) {
                return true;
            }
            if (next == std::string::npos) {
                break;
            }
            index = next + kValueSeparator.size();
        }
        return false;
    }

    std::string joinFilterSummary(const std::vector<std::string>& parts,
                                  const std::string_view separator) {
        std::string summary;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                summary += separator;
            }
            summary += parts[i];
        }
        return summary;
    }

} // namespace ssa::presentation::filterpanel
