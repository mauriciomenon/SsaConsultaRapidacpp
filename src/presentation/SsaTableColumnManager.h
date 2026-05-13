#pragma once

#include "presentation/SsaColumnDisplayCatalog.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssa::presentation {

    inline constexpr int kFallbackTableColumnWidth = 132;

    class SsaTableColumnManager final {
      public:
        void setWidthOverrides(const std::map<std::string, int>& widths);
        void replace(std::vector<std::string> keys, std::vector<SsaDisplayColumn> displayColumns);

        [[nodiscard]] bool empty() const;
        [[nodiscard]] std::size_t count() const;
        [[nodiscard]] bool hasColumn(int column) const;
        [[nodiscard]] bool hasSameKeys(const std::vector<std::string>& keys) const;
        [[nodiscard]] bool
        hasSameMetadata(const std::vector<std::string>& keys,
                        const std::vector<SsaDisplayColumn>& displayColumns) const;

        [[nodiscard]] QString key(int column) const;
        [[nodiscard]] QString label(int column) const;
        [[nodiscard]] int width(int column) const;
        [[nodiscard]] QVariantList widths() const;
        [[nodiscard]] QVariantList tableColumns() const;
        [[nodiscard]] QStringList keys() const;

      private:
        struct ColumnState {
            std::string key;
            SsaDisplayColumn display;
            int width{0};
        };

        [[nodiscard]] std::optional<std::size_t> indexFor(int column) const;
        void rebuildCaches();
        void rebuildWidthCache();

        std::vector<ColumnState> columns_;
        std::unordered_map<std::string, int> widthOverrides_;
        QVariantList widthValues_;
        QVariantList tableColumnValues_;
    };

} // namespace ssa::presentation
