#include "domain/ColumnValuePriorityPolicy.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>

namespace ssa::domain {
    namespace {
        constexpr int kNoPriorityRank = 100;
        constexpr std::array<std::string_view, 2> kLegacyPriorityPrefixes{"SMIN", "SMME"};
        constexpr char asciiUpper(const char value) {
            return value >= 'a' && value <= 'z'
                       ? static_cast<char>(value - static_cast<char>('a' - 'A'))
                       : value;
        }

        constexpr bool isAsciiAlphaNumeric(const char value) {
            return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                   (value >= '0' && value <= '9');
        }

        bool matchesAsciiCodeSegmentCaseInsensitive(const std::string_view value,
                                                    const std::string_view prefix) {
            if (value.size() < prefix.size()) {
                return false;
            }
            for (std::size_t index = 0; index < prefix.size(); ++index) {
                if (asciiUpper(value[index]) != asciiUpper(prefix[index])) {
                    return false;
                }
            }
            return value.size() == prefix.size() || !isAsciiAlphaNumeric(value[prefix.size()]);
        }

        std::string uppercaseAsciiCopy(const std::string_view value) {
            std::string result;
            result.reserve(value.size());
            for (const char ch : value) {
                result.push_back(asciiUpper(ch));
            }
            return result;
        }

        std::optional<unsigned long long> parseUnsignedInteger(const std::string_view value) {
            if (value.empty()) {
                return std::nullopt;
            }
            unsigned long long result = 0;
            for (const char ch : value) {
                if (ch < '0' || ch > '9') {
                    return std::nullopt;
                }
                const auto digit = static_cast<unsigned long long>(ch - '0');
                if (result > (std::numeric_limits<unsigned long long>::max() - digit) / 10) {
                    return std::nullopt;
                }
                result = result * 10 + digit;
            }
            return result;
        }

        int priorityRank(const std::string_view value) {
            for (std::size_t index = 0; index < kOrderedPriorityValues.size(); ++index) {
                if (matchesAsciiCodeSegmentCaseInsensitive(value, kOrderedPriorityValues[index])) {
                    return static_cast<int>(index);
                }
            }
            return kNoPriorityRank;
        }
    } // namespace

    bool isPriorityColumnValue(const std::string_view value) {
        if (priorityRank(value) != kNoPriorityRank) {
            return true;
        }
        return std::ranges::any_of(kLegacyPriorityPrefixes, [value](const auto prefix) {
            return matchesAsciiCodeSegmentCaseInsensitive(value, prefix);
        });
    }

    bool usesPriorityValueOrder(const std::string_view columnKey) {
        return columnKey == "setor_emissor" || columnKey == "setor_executor" ||
               columnKey == "responsavel_programacao" || columnKey == "responsavel_execucao";
    }

    bool columnValueLessForDisplay(const std::string_view left, const std::string_view right) {
        const int leftRank = priorityRank(left);
        const int rightRank = priorityRank(right);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        const auto leftNumber = parseUnsignedInteger(left);
        const auto rightNumber = parseUnsignedInteger(right);
        if (leftNumber.has_value() && rightNumber.has_value() && *leftNumber != *rightNumber) {
            return *leftNumber < *rightNumber;
        }
        const auto leftUpper = uppercaseAsciiCopy(left);
        const auto rightUpper = uppercaseAsciiCopy(right);
        if (leftUpper != rightUpper) {
            return leftUpper < rightUpper;
        }
        return left < right;
    }

} // namespace ssa::domain
