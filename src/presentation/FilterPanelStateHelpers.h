#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "domain/TextFilterToken.h"

#include <QString>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::presentation::filterpanel {

    struct FilterSummaryEntry {
        std::string text;
        std::string kind;
        std::string key;

        bool operator==(const FilterSummaryEntry&) const = default;
    };

    [[nodiscard]] std::optional<int> parseNonNegativeInt(const QString& value);

    [[nodiscard]] std::vector<std::string>
    buildFilterSummaryParts(std::string_view quickSector,
                            const std::map<std::string, std::string>& columnFilters,
                            const domain::AdvancedFilterSpec& advanced);

    [[nodiscard]] std::vector<FilterSummaryEntry>
    buildFilterSummaryEntries(std::string_view quickSector,
                              const std::map<std::string, std::string>& columnFilters,
                              const domain::AdvancedFilterSpec& advanced);

    [[nodiscard]] std::string executorFilterWithQuickSector(std::string_view expression,
                                                            std::string_view quickSector);

    [[nodiscard]] bool hasFilterValue(const std::string& currentFilter,
                                      const std::string& candidateValue);

    [[nodiscard]] std::string joinFilterSummary(const std::vector<std::string>& parts,
                                                std::string_view separator = "  | ");

    // A column status filter split by how the status shortcuts may treat its terms. Exact
    // terms (=X, !=X) read the same as shortcut tokens; any other term (X contains, !X,
    // ^X, $X, ~X) is kept exactly as typed.
    struct StatusFilterTerms {
        domain::TextFilterTokenSet exact;
        std::vector<std::string> typed;
    };

    [[nodiscard]] StatusFilterTerms splitColumnStatusFilter(std::string_view text);

    [[nodiscard]] std::string joinColumnStatusFilter(const StatusFilterTerms& terms);

} // namespace ssa::presentation::filterpanel
