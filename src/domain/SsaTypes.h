#pragma once

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ssa::domain {

    inline constexpr int kMinPageSize = 10;
    inline constexpr int kMaxPageSize = 500;
    inline constexpr int kDefaultPageSize = 50;
    inline constexpr std::string_view kSsaNumberColumnKey = "numero_ssa";
    inline constexpr std::size_t kMaxSafePatternLength = 128;
    inline constexpr bool kDefaultExcludeScaSesSte = false;
    inline constexpr int kFirstIsoWeek = 1;
    inline constexpr int kLastIsoWeek = 53;
    inline constexpr int kYearWeekMultiplier = 100;
    inline constexpr std::size_t kDefaultRequestPageSize = 100;
    inline constexpr std::size_t kDefaultDistinctValuesLimit = 500;

    [[nodiscard]] inline int clampPageSize(const int value) {
        return std::clamp(value, kMinPageSize, kMaxPageSize);
    }

    [[nodiscard]] inline std::size_t pageCount(const std::size_t totalRows,
                                               const std::size_t pageSize) {
        if (pageSize == 0) {
            throw std::invalid_argument("page size must be greater than zero");
        }
        if (totalRows == 0) {
            return 0;
        }
        return (totalRows + pageSize - 1) / pageSize;
    }

    [[nodiscard]] inline bool shouldApplyStatusLastTieBreaker(const std::string_view columnKey) {
        return columnKey == kSsaNumberColumnKey;
    }

    class SsaNumber final {
      public:
        explicit SsaNumber(std::string value) : value_(std::move(value)) {}

        [[nodiscard]] const std::string& value() const noexcept {
            return value_;
        }
        [[nodiscard]] bool empty() const noexcept {
            return value_.empty();
        }

      private:
        std::string value_;
    };

    enum class MatchMode : std::uint8_t {
        Contains,
        StartsWith,
        EndsWith,
        Equals,
        SafePattern,
    };

    struct FilterTerm {
        std::string text;
        MatchMode mode{MatchMode::Contains};
        bool negated{false};
    };

    enum class DerivationFilterMode : std::uint8_t {
        All,
        RootOnly,
        DerivedOnly,
    };

    enum class NumericComparisonMode : std::uint8_t {
        Equals,
        LessOrEqual,
        GreaterOrEqual,
    };

    [[nodiscard]] inline std::string normalizedNumericComparisonMode(const std::string_view value) {
        if (value == "lte" || value == "<=") {
            return "lte";
        }
        if (value == "gte" || value == ">=") {
            return "gte";
        }
        return "eq";
    }

    [[nodiscard]] inline NumericComparisonMode
    numericComparisonModeFromString(const std::string_view value) {
        const auto normalized = normalizedNumericComparisonMode(value);
        if (normalized == "lte") {
            return NumericComparisonMode::LessOrEqual;
        }
        if (normalized == "gte") {
            return NumericComparisonMode::GreaterOrEqual;
        }
        return NumericComparisonMode::Equals;
    }

    [[nodiscard]] inline const char* numericComparisonOperator(const NumericComparisonMode mode) {
        if (mode == NumericComparisonMode::LessOrEqual) {
            return "<=";
        }
        if (mode == NumericComparisonMode::GreaterOrEqual) {
            return ">=";
        }
        return "=";
    }

    struct AdvancedFilterSpec {
        std::string weekColumnKey{"semana_programada"};
        std::map<std::string, std::string> textFilters;
        std::optional<int> year;
        std::optional<int> week;
        std::optional<int> issueYear;
        std::optional<int> executionYear;
        std::optional<int> reprogrammingEquals;
        std::vector<int> reprogrammingValues;
        NumericComparisonMode reprogrammingComparison{NumericComparisonMode::Equals};
        std::optional<int> issueWeekStart;
        std::optional<int> issueWeekEnd;
        std::optional<int> executionWeekStart;
        std::optional<int> executionWeekEnd;
        DerivationFilterMode derivationMode{DerivationFilterMode::All};
        bool onlyReprogrammed{false};

        [[nodiscard]] std::optional<int> exactYearWeek() const {
            if (!year.has_value() || !week.has_value()) {
                return std::nullopt;
            }
            return (*year * kYearWeekMultiplier) + *week;
        }

        [[nodiscard]] std::optional<int> yearStartWeek() const {
            if (!year.has_value()) {
                return std::nullopt;
            }
            return (*year * kYearWeekMultiplier) + kFirstIsoWeek;
        }

        [[nodiscard]] std::optional<int> yearEndWeek() const {
            if (!year.has_value()) {
                return std::nullopt;
            }
            return (*year * kYearWeekMultiplier) + kLastIsoWeek;
        }
    };

    inline bool operator==(const AdvancedFilterSpec& left, const AdvancedFilterSpec& right) {
        return left.weekColumnKey == right.weekColumnKey && left.textFilters == right.textFilters &&
               left.year == right.year && left.week == right.week &&
               left.issueYear == right.issueYear && left.executionYear == right.executionYear &&
               left.reprogrammingEquals == right.reprogrammingEquals &&
               left.reprogrammingValues == right.reprogrammingValues &&
               left.reprogrammingComparison == right.reprogrammingComparison &&
               left.issueWeekStart == right.issueWeekStart &&
               left.issueWeekEnd == right.issueWeekEnd &&
               left.executionWeekStart == right.executionWeekStart &&
               left.executionWeekEnd == right.executionWeekEnd &&
               left.derivationMode == right.derivationMode &&
               left.onlyReprogrammed == right.onlyReprogrammed;
    }

    struct SsaFilterExpression {
        std::vector<FilterTerm> generalTerms;
        std::map<std::string, std::vector<FilterTerm>> columnTerms;
        std::optional<std::string> quickSector;
        bool excludeScaSesSte{kDefaultExcludeScaSesSte};
        AdvancedFilterSpec advanced;
    };

    struct SsaRecord {
        using Schema = std::vector<std::string>;
        struct TransparentStringHash {
            using is_transparent = void;

            [[nodiscard]] std::size_t operator()(const std::string_view value) const noexcept {
                return std::hash<std::string_view>{}(value);
            }

            [[nodiscard]] std::size_t operator()(const std::string& value) const noexcept {
                return std::hash<std::string_view>{}(value);
            }
        };
        struct TransparentStringEqual {
            using is_transparent = void;

            [[nodiscard]] bool operator()(const std::string_view left,
                                          const std::string_view right) const noexcept {
                return left == right;
            }
        };
        struct SchemaIndex {
            Schema keys;
            std::unordered_map<std::string, std::size_t, TransparentStringHash,
                               TransparentStringEqual>
                indexByKey;
        };
        struct FieldView {
            std::string_view key;
            std::string_view value;
        };

        SsaRecord() = default;

        SsaRecord(std::shared_ptr<const SchemaIndex> schema, std::vector<std::string> rowValues)
            : schema_(std::move(schema)), rowValues_(std::move(rowValues)) {
            if (!schema_ && !rowValues_.empty()) {
                throw std::invalid_argument("SSA record values require a schema");
            }
            if (schema_ && schema_->keys.size() != rowValues_.size()) {
                throw std::invalid_argument("SSA record schema/value count mismatch");
            }
        }

        explicit SsaRecord(const std::map<std::string, std::string>& fields) {
            auto mutableSchema = std::make_shared<SchemaIndex>();
            mutableSchema->keys.reserve(fields.size());
            mutableSchema->indexByKey.reserve(fields.size());
            rowValues_.reserve(fields.size());
            std::size_t index = 0;
            for (const auto& [key, value] : fields) {
                mutableSchema->indexByKey.emplace(key, index);
                mutableSchema->keys.push_back(key);
                rowValues_.push_back(value);
                ++index;
            }
            schema_ = std::move(mutableSchema);
        }

        [[nodiscard]] std::string_view valueOf(const std::string_view key) const {
            if (!schema_) {
                return {};
            }
            const auto schemaEntry = schema_->indexByKey.find(key);
            if (schemaEntry == schema_->indexByKey.end()) {
                return {};
            }
            const auto index = schemaEntry->second;
            if (index >= rowValues_.size()) {
                throw std::logic_error("SSA record schema index is out of bounds");
            }
            return std::string_view{rowValues_[index]};
        }

        [[nodiscard]] std::string_view valueAt(const std::size_t index) const {
            return index < rowValues_.size() ? std::string_view{rowValues_[index]}
                                             : std::string_view{};
        }

        [[nodiscard]] std::vector<FieldView> fields() const {
            std::vector<FieldView> result;
            if (!schema_) {
                return result;
            }
            const auto count = std::min(schema_->keys.size(), rowValues_.size());
            result.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                result.push_back({schema_->keys[index], rowValues_[index]});
            }
            return result;
        }

        [[nodiscard]] std::size_t fieldCount() const {
            return schema_ ? std::min(schema_->keys.size(), rowValues_.size()) : 0;
        }

      private:
        std::shared_ptr<const SchemaIndex> schema_;
        std::vector<std::string> rowValues_;
    };

    struct SortSpec {
        std::string columnKey{"numero_ssa"};
        bool ascending{false};
        bool statusLast{shouldApplyStatusLastTieBreaker(columnKey)};
    };

    inline bool operator==(const SortSpec& left, const SortSpec& right) {
        return left.columnKey == right.columnKey && left.ascending == right.ascending &&
               left.statusLast == right.statusLast;
    }

    struct SsaPageRequest {
        std::size_t pageIndex{0};
        std::size_t pageSize{kDefaultRequestPageSize};
        std::string searchText;
        std::map<std::string, std::string> columnFilters;
        std::string quickSector;
        bool excludeScaSesSte{kDefaultExcludeScaSesSte};
        AdvancedFilterSpec advancedFilters;
        SortSpec sort;
        std::vector<std::string> visibleColumns;
    };

    inline bool operator==(const SsaPageRequest& left, const SsaPageRequest& right) {
        return left.pageIndex == right.pageIndex && left.pageSize == right.pageSize &&
               left.searchText == right.searchText && left.columnFilters == right.columnFilters &&
               left.quickSector == right.quickSector &&
               left.excludeScaSesSte == right.excludeScaSesSte &&
               left.advancedFilters == right.advancedFilters && left.sort == right.sort &&
               left.visibleColumns == right.visibleColumns;
    }

    struct SsaPageResult {
        std::vector<SsaRecord> rows;
        std::size_t totalRows{0};
        std::size_t pageIndex{0};
        std::size_t pageSize{kDefaultRequestPageSize};
    };

    struct DistinctValuesRequest {
        std::string columnKey;
        SsaFilterExpression filter;
        std::size_t limit{kDefaultDistinctValuesLimit};
        bool orderByFrequency{false};
    };

} // namespace ssa::domain
