#include "presentation/FilterPreferencesNormalizer.h"

#include "domain/ColumnCatalog.h"
#include "domain/TextFilterToken.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace ssa::presentation {

    namespace {
        void normalizeQuickSector(ports::FilterPreferencesSnapshot& filters) {
            if (filters.quickSector.empty()) {
                return;
            }
            const auto executorKey = std::string{domain::ColumnCatalog::executorColumnKey()};
            auto& executorExpression = filters.advancedTextFilters[executorKey];
            auto tokens = domain::parseTextFilterTokens(executorExpression);
            domain::addTextFilterValue(tokens, filters.quickSector,
                                       domain::TextFilterOperator::Equals);
            executorExpression = domain::joinTextFilterTokens(tokens);
            filters.quickSector.clear();
        }

        void normalizeColumnOverlap(ports::FilterPreferencesSnapshot& filters) {
            for (const auto& filter : filters.advancedTextFilters) {
                filters.columnFilters.erase(filter.first);
            }
        }

        bool includesExcludedStatus(const std::map<std::string, std::string>& textFilters) {
            const auto statusKey = std::string{domain::ColumnCatalog::statusColumnKey()};
            const auto filter = textFilters.find(statusKey);
            return filter != textFilters.end() &&
                   domain::ColumnCatalog::containsExcludedStatusCode(filter->second);
        }

        void normalizeStatusExclusion(ports::FilterPreferencesSnapshot& filters) {
            if (!filters.excludeScaSesSte) {
                return;
            }
            if (!includesExcludedStatus(filters.advancedTextFilters) &&
                !includesExcludedStatus(filters.columnFilters)) {
                return;
            }
            filters.excludeScaSesSte = false;
        }
    } // namespace

    void normalizeFilterPreferences(ports::FilterPreferencesSnapshot& filters) {
        normalizeQuickSector(filters);
        normalizeColumnOverlap(filters);
        normalizeStatusExclusion(filters);
    }

    void normalizeSavedFilterPreferences(std::vector<ports::SavedFilterSnapshot>& filters) {
        for (auto& saved : filters) {
            normalizeFilterPreferences(saved.filters);
        }
    }

} // namespace ssa::presentation
