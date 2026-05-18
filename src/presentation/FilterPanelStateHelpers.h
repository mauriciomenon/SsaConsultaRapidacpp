#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <QString>

#include <optional>
#include <string_view>
#include <vector>

namespace ssa::presentation::filterpanel {

    [[nodiscard]] std::optional<int> parsePositiveInt(const QString& value);

    [[nodiscard]] domain::DerivationFilterMode derivationModeFromString(const QString& value);

    [[nodiscard]] std::string derivationModeToString(const domain::DerivationFilterMode mode);

    [[nodiscard]] std::vector<std::string>
    buildFilterSummaryParts(const std::string_view quickSector,
                            const std::map<std::string, std::string>& columnFilters,
                            const domain::AdvancedFilterSpec& advanced);
    [[nodiscard]] bool hasFilterValue(const std::string& currentFilter,
                                      const std::string& candidateValue);

    [[nodiscard]] std::string joinFilterSummary(const std::vector<std::string>& parts,
                                                std::string_view separator = "  | ");

} // namespace ssa::presentation::filterpanel
