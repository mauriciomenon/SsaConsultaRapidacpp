#include "domain/ColumnValuePriorityPolicy.h"

#include <algorithm>
#include <array>

namespace ssa::domain {
    namespace {
        constexpr std::array<std::string_view, 2> kPriorityPrefixes{"SMIN", "SMME"};

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
    } // namespace

    bool isPriorityColumnValue(const std::string_view value) {
        return std::ranges::any_of(kPriorityPrefixes, [value](const auto prefix) {
            return matchesAsciiCodeSegmentCaseInsensitive(value, prefix);
        });
    }

} // namespace ssa::domain
