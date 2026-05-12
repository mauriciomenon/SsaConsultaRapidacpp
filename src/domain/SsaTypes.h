#pragma once

#include "domain/ColumnCatalog.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    inline constexpr std::string_view kScaSesSteExclusionSummary = "sem SCA/SES/STE";

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
