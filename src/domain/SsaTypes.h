#pragma once

#include "domain/ColumnCatalog.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace ssa::domain {

    inline constexpr std::string_view kScaSesSteExclusionSummary = "sem SCA/SES/STE";
    inline constexpr int kMinPageSize = 10;
    inline constexpr int kMaxPageSize = 500;
    inline constexpr int kDefaultPageSize = 100;
    inline constexpr int kMinDetailsPanelWidth = 280;
    inline constexpr int kMaxDetailsPanelWidth = 680;
    inline constexpr int kDefaultDetailsPanelWidth = 360;
    inline constexpr std::string_view kStatusLastSortColumn = "numero_ssa";
    inline constexpr bool kDefaultExcludeScaSesSte = true;

    [[nodiscard]] inline int clampPageSize(const int value) {
        return std::clamp(value, kMinPageSize, kMaxPageSize);
    }

    [[nodiscard]] inline int clampDetailsPanelWidth(const int value) {
        return std::clamp(value, kMinDetailsPanelWidth, kMaxDetailsPanelWidth);
    }

    [[nodiscard]] inline bool requiresStatusLastSort(const std::string_view columnKey) {
        return columnKey == kStatusLastSortColumn;
    }

    [[nodiscard]] inline std::vector<std::string> filterSummaryParts(
        const std::string_view quickSector,
        const bool excludeScaSesSte,
        const std::map<std::string, std::string>& columnFilters) {
        std::vector<std::string> parts;
        if (!quickSector.empty()) {
            parts.push_back(std::string("setor_executor:") + std::string(quickSector));
        }
        if (excludeScaSesSte) {
            parts.push_back(std::string(kScaSesSteExclusionSummary));
        }
        for (const auto& [key, value] : columnFilters) {
            parts.push_back(key + ":" + value);
        }
        return parts;
    }

    [[nodiscard]] inline std::string joinFilterSummary(
        const std::vector<std::string>& parts,
        const std::string_view separator = "  | ") {
        std::string summary;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                summary += separator;
            }
            summary += parts[i];
        }
        return summary;
    }

    class SsaId final {
      public:
        explicit SsaId(std::string value) : value_(std::move(value)) {}

        [[nodiscard]] const std::string& value() const noexcept {
            return value_;
        }
        [[nodiscard]] bool empty() const noexcept {
            return value_.empty();
        }

      private:
        std::string value_;
    };

    enum class MatchMode {
        Contains,
        StartsWith,
        EndsWith,
        Equals,
        Regex,
    };

    struct FilterTerm {
        std::string text;
        MatchMode mode{MatchMode::Contains};
        bool negated{false};
    };

    struct SsaFilterExpression {
        std::vector<FilterTerm> generalTerms;
        std::map<std::string, std::vector<FilterTerm>> columnTerms;
        std::optional<std::string> quickSector;
        bool excludeScaSesSte{true};
    };

    struct SsaRecord {
        std::map<std::string, std::string> values;

        [[nodiscard]] std::string valueOf(const std::string& key) const {
            const auto it = values.find(key);
            return it == values.end() ? std::string{} : it->second;
        }
    };

    struct SortSpec {
        std::string columnKey{"numero_ssa"};
        bool ascending{false};
        bool statusLast{true};
    };

    struct SsaPageRequest {
        std::size_t pageIndex{0};
        std::size_t pageSize{100};
        std::string searchText;
        std::map<std::string, std::string> columnFilters;
        std::string quickSector;
        bool excludeScaSesSte{true};
        SortSpec sort;
        std::vector<std::string> visibleColumns;
    };

    struct SsaPageResult {
        std::vector<SsaRecord> rows;
        std::size_t totalRows{0};
        std::size_t pageIndex{0};
        std::size_t pageSize{100};
    };

    struct DistinctValuesRequest {
        std::string columnKey;
        SsaFilterExpression filter;
        std::size_t limit{500};
    };

} // namespace ssa::domain
