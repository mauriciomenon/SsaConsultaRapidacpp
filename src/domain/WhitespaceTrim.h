#pragma once

#include "domain/AsciiText.h"

#include <string>
#include <string_view>

namespace ssa::domain {

    // Trims ASCII whitespace and the UTF-8 no-break space (C2 A0) that spreadsheets often carry,
    // matching Python str.strip(); a multi-byte sequence is never split.
    [[nodiscard]] inline std::string trimWhitespace(std::string_view value) {
        constexpr std::string_view kNoBreakSpace{"\xC2\xA0"};
        while (!value.empty()) {
            if (ascii::isSpace(value.front())) {
                value.remove_prefix(1);
            } else if (value.starts_with(kNoBreakSpace)) {
                value.remove_prefix(kNoBreakSpace.size());
            } else {
                break;
            }
        }
        while (!value.empty()) {
            if (ascii::isSpace(value.back())) {
                value.remove_suffix(1);
            } else if (value.ends_with(kNoBreakSpace)) {
                value.remove_suffix(kNoBreakSpace.size());
            } else {
                break;
            }
        }
        return std::string{value};
    }

} // namespace ssa::domain
