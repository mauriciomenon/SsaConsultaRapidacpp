#include "infra/import/SsaImportConflictResolver.h"

#include "domain/SsaTypes.h"

#include <unordered_map>
#include <unordered_set>

namespace ssa::infra::importing {

    ResolvedSsaImportRows
    SsaImportConflictResolver::resolveForDeleteInsertUpsertBySsaNumberKeepingUnkeyedRows(
        const std::vector<SsaImportBatch>& batches) const {
        const auto ssaNumberKey = std::string{domain::kSsaNumberColumnKey};
        ResolvedSsaImportRows resolved;
        std::unordered_map<std::string, std::size_t> indexBySsa;
        std::unordered_set<std::string> seenNumbers;
        for (const auto& batch : batches) {
            for (const auto& row : batch.rows) {
                auto number = rowValue(row, ssaNumberKey);
                if (number.empty()) {
                    resolved.rows.push_back(row);
                    continue;
                }
                if (!seenNumbers.contains(number)) {
                    seenNumbers.insert(number);
                    resolved.ssaNumbersForUpsertDelete.push_back(number);
                }
                const auto existing = indexBySsa.find(number);
                if (existing == indexBySsa.end()) {
                    indexBySsa.emplace(std::move(number), resolved.rows.size());
                    resolved.rows.push_back(row);
                    continue;
                }
                resolved.rows[existing->second] = row;
                ++resolved.duplicateRows;
            }
        }
        return resolved;
    }

} // namespace ssa::infra::importing
