#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    enum class ColumnType {
        Text,
        Integer,
        DateText,
    };

    struct ColumnDef {
        std::string key;
        std::string label;
        ColumnType type;
        bool defaultVisible;
        bool generalSearch;
        int defaultWidth;
    };

    class ColumnCatalog final {
      public:
        [[nodiscard]] static std::span<const ColumnDef> all();
        [[nodiscard]] static std::vector<ColumnDef> defaultVisible();
        [[nodiscard]] static std::vector<std::string> defaultVisibleKeys();
        [[nodiscard]] static std::vector<std::string>
        visibleKeysOrDefault(std::vector<std::string> keys);
        [[nodiscard]] static std::vector<std::string> generalSearchKeys();
        [[nodiscard]] static std::vector<std::string> filterColumnKeys();
        [[nodiscard]] static std::string defaultFilterColumnKey();
        [[nodiscard]] static std::string_view statusColumnKey();
        [[nodiscard]] static std::string_view executorColumnKey();
        [[nodiscard]] static std::string_view derivationColumnKey();
        [[nodiscard]] static std::span<const std::string_view> excludedStatusCodes();
        [[nodiscard]] static std::span<const std::string_view> weekColumnKeys();
        [[nodiscard]] static std::span<const std::string_view> reprogrammingColumnKeys();
        [[nodiscard]] static std::string_view statusLastSortCode();
        [[nodiscard]] static bool isQuickSectorFilterColumn(std::string_view key);
        [[nodiscard]] static bool isStatusExclusionFilterColumn(std::string_view key);
        [[nodiscard]] static std::optional<ColumnDef> find(std::string_view key);
        [[nodiscard]] static bool contains(std::string_view key);
    };

} // namespace ssa::domain
