#pragma once

#include "domain/SsaImportPolicy.h"

#include <charconv>
#include <span>
#include <string>
#include <string_view>

namespace ssa::domain {

    class ExecutionHistoryPolicy final {
      public:
        [[nodiscard]] static std::string
        targetColumn(const SsaImportPolicy::Values& existing,
                     std::span<const std::string> availableColumns) {
            constexpr std::string_view baseColumn = "descricao_execucao";
            const auto containsColumn = [&](const std::string_view key) {
                for (const auto& column : availableColumns) {
                    if (column == key) {
                        return true;
                    }
                }
                return false;
            };
            if (!containsColumn(baseColumn)) {
                return std::string{baseColumn};
            }
            const auto valueFor = [&](const std::string_view key) {
                const auto found = existing.find(std::string{key});
                return found == existing.end() ? std::string{} : found->second;
            };
            std::size_t lastUsed = valueFor(baseColumn).empty() ? 0 : 1;
            for (const auto& column : availableColumns) {
                constexpr std::size_t prefixSize = baseColumn.size() + 1;
                if (!column.starts_with(baseColumn) || column.size() <= prefixSize ||
                    column[baseColumn.size()] != '_') {
                    continue;
                }
                std::size_t suffix = 0;
                const auto text = std::string_view{column}.substr(prefixSize);
                const auto [end, error] =
                    std::from_chars(text.data(), text.data() + text.size(), suffix);
                if (error == std::errc{} && end == text.data() + text.size() && suffix >= 2 &&
                    suffix > lastUsed && !valueFor(column).empty()) {
                    lastUsed = suffix;
                }
            }
            if (lastUsed == 0) {
                return std::string{baseColumn};
            }
            const auto candidate = std::string{baseColumn} + '_' + std::to_string(lastUsed + 1);
            return containsColumn(candidate) ? candidate : std::string{baseColumn};
        }
    };

} // namespace ssa::domain
