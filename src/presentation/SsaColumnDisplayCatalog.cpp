#include "presentation/SsaColumnDisplayCatalog.h"

namespace ssa::presentation {

    SsaDisplayColumn SsaColumnDisplayCatalog::resolve(const std::string& key) const {
        const auto column = domain::ColumnCatalog::find(key);
        if (!column) {
            return {key, key, {}, domain::ColumnType::Text, 132};
        }
        return {column->key, column->label, column->labelFull, column->type, column->defaultWidth};
    }

    std::vector<SsaDisplayColumn>
    SsaColumnDisplayCatalog::resolveAll(const std::vector<std::string>& keys) const {
        std::vector<SsaDisplayColumn> columns;
        columns.reserve(keys.size());
        for (const auto& key : keys) {
            columns.push_back(resolve(key));
        }
        return columns;
    }

} // namespace ssa::presentation
