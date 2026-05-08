#pragma once

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
        [[nodiscard]] static std::vector<std::string> generalSearchKeys();
        [[nodiscard]] static std::optional<ColumnDef> find(std::string_view key);
        [[nodiscard]] static bool contains(std::string_view key);
    };

} // namespace ssa::domain
