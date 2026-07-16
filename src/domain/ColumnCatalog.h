#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    enum class ColumnType : std::uint8_t {
        Text,
        Integer,
        DateText,
    };

    struct ColumnDef {
        std::string key;
        std::string label;
        std::string labelFull;
        ColumnType type = ColumnType::Text;
        bool defaultVisible = false;
        bool generalSearch = false;
        int defaultWidth = 0;
    };

    class ColumnCatalog final {
      public:
        [[nodiscard]] static constexpr std::string_view schemaTableName() noexcept {
            return "ssa_table";
        }
        [[nodiscard]] static constexpr int schemaVersion() noexcept {
            return 1;
        }
        [[nodiscard]] static std::vector<ColumnDef> schemaColumns();
        [[nodiscard]] static std::span<const std::string_view> requiredSchemaColumns();
        [[nodiscard]] static std::span<const ColumnDef> all();
        [[nodiscard]] static std::vector<ColumnDef> storageColumns();
        [[nodiscard]] static std::vector<std::string> defaultVisibleKeys();
        [[nodiscard]] static std::vector<std::string>
        visibleKeysOrDefault(std::vector<std::string> keys);
        [[nodiscard]] static std::vector<std::string> generalSearchKeys();
        [[nodiscard]] static const std::vector<std::string>& orderedFilterColumnKeys();
        [[nodiscard]] static std::span<const std::string_view> advancedFilterKeys();
        [[nodiscard]] static std::string_view advancedFilterLabel(std::string_view key);
        [[nodiscard]] static std::string_view advancedFilterShortLabel(std::string_view key);
        [[nodiscard]] static std::string defaultFilterColumnKey();
        [[nodiscard]] static std::string_view statusColumnKey();
        [[nodiscard]] static std::string_view executorColumnKey();
        [[nodiscard]] static std::string_view derivationColumnKey();
        [[nodiscard]] static std::string_view derivedCountColumnKey();
        [[nodiscard]] static std::span<const std::string_view> statusShortcutCodes();
        [[nodiscard]] static std::string_view downloadableStatusFilterExpression();
        [[nodiscard]] static std::span<const std::string_view> excludedStatusCodes();
        [[nodiscard]] static bool containsExcludedStatusCode(std::string_view filterExpression);
        [[nodiscard]] static std::span<const std::string_view> weekColumnKeys();
        [[nodiscard]] static std::string_view defaultAdvancedWeekColumnKey();
        [[nodiscard]] static std::string_view issueWeekColumnKey();
        [[nodiscard]] static std::string_view executionWeekColumnKey();
        [[nodiscard]] static std::span<const std::string_view> reprogrammingColumnKeys();
        [[nodiscard]] static std::string_view primaryReprogrammingColumnKey();
        [[nodiscard]] static std::string_view statusLastSortCode();
        [[nodiscard]] static bool isQuickSectorFilterColumn(std::string_view key);
        [[nodiscard]] static bool isStatusExclusionFilterColumn(std::string_view key);
        [[nodiscard]] static bool isReprogrammingColumn(std::string_view key);
        [[nodiscard]] static bool isDerivedCountColumn(std::string_view key);
        [[nodiscard]] static const ColumnDef* find(std::string_view key);
        [[nodiscard]] static bool contains(std::string_view key);
    };

} // namespace ssa::domain
