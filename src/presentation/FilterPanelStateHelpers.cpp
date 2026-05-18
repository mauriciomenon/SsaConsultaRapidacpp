#include "presentation/FilterPanelStateHelpers.h"

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

        void appendRangeSummary(std::vector<std::string>& parts, const std::string_view label,
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
            parts.push_back(std::move(part));
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
        std::vector<std::string> parts;
        if (!quickSector.empty()) {
            parts.push_back(std::string(domain::ColumnCatalog::executorColumnKey()) + ":" +
                            std::string(quickSector));
        }
        for (const auto& [key, value] : columnFilters) {
            auto part = key;
            part += ":";
            part += value;
            parts.push_back(std::move(part));
        }
        for (const auto& [key, value] : advanced.textFilters) {
            std::string part{"adv "};
            part += key;
            part += ":";
            part += value;
            parts.push_back(std::move(part));
        }
        if (advanced.year.has_value()) {
            parts.push_back("ano:" + std::to_string(*advanced.year));
        }
        if (advanced.week.has_value()) {
            parts.push_back("semana:" + std::to_string(*advanced.week));
        }
        if (advanced.issueYear.has_value()) {
            parts.push_back("ano emissao:" + std::to_string(*advanced.issueYear));
        }
        if (advanced.executionYear.has_value()) {
            parts.push_back("ano execucao:" + std::to_string(*advanced.executionYear));
        }
        if (advanced.reprogrammingEquals.has_value()) {
            parts.push_back("reprog=" + std::to_string(*advanced.reprogrammingEquals));
        }
        appendRangeSummary(parts, "emissao", advanced.issueWeekStart, advanced.issueWeekEnd);
        appendRangeSummary(parts, "execucao", advanced.executionWeekStart,
                           advanced.executionWeekEnd);
        if (advanced.derivationMode == domain::DerivationFilterMode::RootOnly) {
            parts.emplace_back("somente originais");
        } else if (advanced.derivationMode == domain::DerivationFilterMode::DerivedOnly) {
            parts.emplace_back("somente derivadas");
        }
        if (advanced.onlyReprogrammed) {
            parts.emplace_back("somente reprogramadas");
        }
        return parts;
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
