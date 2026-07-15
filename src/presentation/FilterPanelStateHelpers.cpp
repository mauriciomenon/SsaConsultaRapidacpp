#include "presentation/FilterPanelStateHelpers.h"

#include "domain/ColumnCatalog.h"
#include "domain/TextFilterToken.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation::filterpanel {

    namespace {

        constexpr std::string_view kValueSeparator = ",";
        constexpr std::string_view kSpaceChars = " \t\r\n";

        struct RangeSummaryRequest {
            std::string_view label;
            std::string_view kind;
            std::optional<int> start;
            std::optional<int> end;
        };

        [[nodiscard]] std::string_view trimCopy(std::string_view value) {
            auto begin = value.find_first_not_of(kSpaceChars);
            if (begin == std::string_view::npos) {
                return {};
            }
            const auto end = value.find_last_not_of(kSpaceChars) + 1;
            return value.substr(begin, end - begin);
        }

        void appendRangeSummary(std::vector<FilterSummaryEntry>& entries,
                                const RangeSummaryRequest& request) {
            if (!request.start.has_value() && !request.end.has_value()) {
                return;
            }
            std::string part{request.label};
            if (request.start.has_value() && request.end.has_value()) {
                part += ":";
                part += std::to_string(*request.start);
                part += "-";
                part += std::to_string(*request.end);
            } else if (request.start.has_value()) {
                part += ">=";
                part += std::to_string(*request.start);
            } else {
                part += "<=";
                part += std::to_string(*request.end);
            }
            entries.push_back(
                {.text = std::move(part), .kind = std::string{request.kind}, .key = {}});
        }

        std::string shortColumnLabel(const std::string_view key) {
            auto label = std::string{domain::ColumnCatalog::advancedFilterShortLabel(key)};
            if (!label.empty() && label.back() == '.') {
                label.pop_back();
            }
            return label;
        }

        // Render a text-filter token expression (=value,!value) into a
        // friendly chip label: included values first (comma-joined), then an
        // "Exc:" group for excluded values. Mirrors the Python reference
        // (filter_summary_advanced.add_adv with op handling).
        // Example: "=IEE3,!SES,!STE" -> "IEE3, Exc: SES, STE".
        std::string formatTextFilterValue(const std::string& value) {
            const auto tokens = domain::parseTextFilterTokens(value);
            if (tokens.ordered.empty()) {
                return value;
            }
            std::string included;
            std::string excluded;
            for (const auto& token : tokens.ordered) {
                if (token.filterOperator == domain::TextFilterOperator::Equals) {
                    if (!included.empty()) {
                        included += ", ";
                    }
                    included += token.value;
                } else {
                    if (!excluded.empty()) {
                        excluded += ", ";
                    }
                    excluded += token.value;
                }
            }
            std::string result;
            if (!included.empty()) {
                result = std::move(included);
            }
            if (!excluded.empty()) {
                if (!result.empty()) {
                    result += ", ";
                }
                result += "Exc: ";
                result += excluded;
            }
            return result;
        }

        void appendTextSummary(std::vector<FilterSummaryEntry>& entries, const std::string& key,
                               const std::string& value, std::string kind) {
            if (value.empty()) {
                return;
            }
            std::string part{shortColumnLabel(key)};
            part += ": ";
            part += formatTextFilterValue(value);
            entries.push_back({.text = std::move(part), .kind = std::move(kind), .key = key});
        }

    } // namespace

    std::optional<int> parsePositiveInt(const QString& value) {
        const auto text = value.trimmed();
        if (text.isEmpty()) {
            return std::nullopt;
        }
        bool conversionOk = false;
        const int parsed = text.toInt(&conversionOk);
        if (!conversionOk || parsed <= 0) {
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

    std::string derivationModeToString(domain::DerivationFilterMode mode) {
        if (mode == domain::DerivationFilterMode::RootOnly) {
            return "root";
        }
        if (mode == domain::DerivationFilterMode::DerivedOnly) {
            return "derived";
        }
        return "all";
    }

    std::vector<std::string>
    buildFilterSummaryParts(std::string_view quickSector,
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
    buildFilterSummaryEntries(std::string_view quickSector,
                              const std::map<std::string, std::string>& columnFilters,
                              const domain::AdvancedFilterSpec& advanced) {
        std::vector<FilterSummaryEntry> entries;
        const auto executorKey = std::string{domain::ColumnCatalog::executorColumnKey()};
        const auto executorFilter = advanced.textFilters.find(executorKey);
        const bool hasExecutorText =
            executorFilter != advanced.textFilters.end() && !executorFilter->second.empty();
        if (!quickSector.empty() && hasExecutorText) {
            appendTextSummary(entries, executorKey,
                              executorFilterWithQuickSector(executorFilter->second, quickSector),
                              "executor_combined");
        } else if (!quickSector.empty()) {
            entries.push_back(
                {.text = shortColumnLabel(domain::ColumnCatalog::executorColumnKey()) + ": " +
                         std::string(quickSector),
                 .kind = "quick_sector",
                 .key = {}});
        }
        for (const auto& [key, value] : columnFilters) {
            auto part = shortColumnLabel(key);
            part += ": ";
            part += formatTextFilterValue(value);
            entries.push_back({.text = std::move(part), .kind = "column", .key = key});
        }
        for (const auto& [key, value] : advanced.textFilters) {
            if (key == executorKey && !quickSector.empty()) {
                continue;
            }
            appendTextSummary(entries, key, value, "advanced_text");
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
        if (!advanced.reprogrammingValues.empty()) {
            std::string summary;
            if (advanced.reprogrammingComparison == domain::NumericComparisonMode::Equals) {
                summary = "reprog valores:";
                for (std::size_t index = 0; index < advanced.reprogrammingValues.size(); ++index) {
                    if (index > 0) {
                        summary += ",";
                    }
                    summary += std::to_string(advanced.reprogrammingValues[index]);
                }
            } else {
                const auto [minIt, maxIt] =
                    std::ranges::minmax_element(advanced.reprogrammingValues);
                const int threshold =
                    advanced.reprogrammingComparison == domain::NumericComparisonMode::LessOrEqual
                        ? *maxIt
                        : *minIt;
                summary = std::string{"reprog"} +
                          domain::numericComparisonOperator(advanced.reprogrammingComparison) +
                          std::to_string(threshold);
            }
            entries.push_back(
                {.text = std::move(summary), .kind = "advanced_reprogramming", .key = {}});
        }
        appendRangeSummary(entries, {.label = "emissao",
                                     .kind = "advanced_issue_week_range",
                                     .start = advanced.issueWeekStart,
                                     .end = advanced.issueWeekEnd});
        appendRangeSummary(entries, {.label = "execucao",
                                     .kind = "advanced_execution_week_range",
                                     .start = advanced.executionWeekStart,
                                     .end = advanced.executionWeekEnd});
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

    std::string executorFilterWithQuickSector(const std::string_view expression,
                                              const std::string_view quickSector) {
        auto tokens = domain::parseTextFilterTokens(expression);
        domain::addTextFilterValue(tokens, quickSector, domain::TextFilterOperator::Equals);
        return domain::joinTextFilterTokens(tokens);
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
                                  std::string_view separator) {
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
