#include "presentation/SsaTablePageFormatter.h"

#include "presentation/SsaRecordValueFormatter.h"

#include <memory>
#include <stdexcept>
#include <vector>

namespace ssa::presentation {

    std::optional<SsaTableDisplayValues>
    SsaTablePageFormatter::format(const domain::SsaPageResult& page,
                                  const std::vector<SsaDisplayColumn>& displayColumns,
                                  const std::shared_ptr<std::atomic_bool>& cancelToken) {
        if (page.rows.size() > static_cast<std::size_t>(domain::kMaxPageSize)) {
            throw std::length_error("Pagina excede limite de exibicao");
        }

        SsaTableDisplayValues display;
        display.rowCount = page.rows.size();
        display.columnCount = displayColumns.size();
        display.values.resize(display.rowCount * display.columnCount);
        std::size_t valueIndex = 0;
        for (const auto& row : page.rows) {
            for (const auto& column : displayColumns) {
                if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
                    return std::nullopt;
                }
                display.values[valueIndex] =
                    SsaRecordValueFormatter::valueFor(row, column.key, column.type);
                ++valueIndex;
            }
        }
        return display;
    }

} // namespace ssa::presentation
