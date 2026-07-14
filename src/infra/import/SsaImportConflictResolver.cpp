#include "infra/import/SsaImportConflictResolver.h"

#include "domain/SsaImportPolicy.h"
#include "domain/SsaTypes.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ssa::infra::importing {

    ResolvedSsaImportRows SsaImportConflictResolver::resolveBySsaNumberKeepingUnkeyedRows(
        const std::vector<SsaImportBatch>& batches) const {
        const auto ssaNumberKey = std::string{domain::kSsaNumberColumnKey};
        ResolvedSsaImportRows resolved;
        std::unordered_map<std::string, std::size_t> indexBySsa;
        std::unordered_set<std::string> conflictingNumbers;
        for (const auto& batch : batches) {
            for (const auto& row : batch.rows) {
                auto number = rowValue(row, ssaNumberKey);
                if (number.empty()) {
                    resolved.rows.push_back(row);
                    continue;
                }
                if (conflictingNumbers.contains(number)) {
                    ++resolved.duplicateRows;
                    continue;
                }
                const auto existing = indexBySsa.find(number);
                if (existing == indexBySsa.end()) {
                    indexBySsa.emplace(std::move(number), resolved.rows.size());
                    resolved.rows.push_back(row);
                    continue;
                }
                const auto merged =
                    domain::SsaImportPolicy::merge(resolved.rows[existing->second], row);
                ++resolved.duplicateRows;
                if (merged.conflict) {
                    conflictingNumbers.insert(number);
                    ++resolved.conflictRows;
                    continue;
                }
                resolved.rows[existing->second] = merged.values;
            }
        }
        std::erase_if(resolved.rows, [&](const auto& row) {
            return conflictingNumbers.contains(rowValue(row, ssaNumberKey));
        });
        return resolved;
    }

} // namespace ssa::infra::importing
