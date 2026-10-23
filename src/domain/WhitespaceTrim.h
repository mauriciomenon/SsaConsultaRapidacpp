#pragma once

#include <array>
#include <string>
#include <string_view>

namespace ssa::domain {

    namespace whitespace_detail {

        // Python str.isspace(): \t..\r, \x1c..\x1f and the space.
        [[nodiscard]] constexpr bool isAsciiSpace(const char ch) noexcept {
            return (ch >= '\t' && ch <= '\r') || (ch >= '\x1c' && ch <= ' ');
        }

        // The non-ASCII code points Python strips, in UTF-8: U+0085, U+00A0, U+1680,
        // U+2000..U+200A, U+2028, U+2029, U+202F, U+205F, U+3000.
        inline constexpr std::array<std::string_view, 18> kUnicodeSpaces{
            "\xC2\x85",     "\xC2\xA0",     "\xE1\x9A\x80", "\xE2\x80\x80", "\xE2\x80\x81",
            "\xE2\x80\x82", "\xE2\x80\x83", "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86",
            "\xE2\x80\x87", "\xE2\x80\x88", "\xE2\x80\x89", "\xE2\x80\x8A", "\xE2\x80\xA8",
            "\xE2\x80\xA9", "\xE2\x80\xAF", "\xE2\x81\x9F",
        };
        inline constexpr std::string_view kIdeographicSpace{"\xE3\x80\x80"};

        // Length of the whitespace at the front (or back) of value, 0 when there is none; a
        // multi-byte sequence is matched whole, never split.
        [[nodiscard]] constexpr std::size_t leadingSpace(const std::string_view value) noexcept {
            const auto lead = static_cast<unsigned char>(value.front());
            if (lead < 0x80) {
                return isAsciiSpace(value.front()) ? 1 : 0;
            }
            if (value.starts_with(kIdeographicSpace)) {
                return kIdeographicSpace.size();
            }
            for (const auto space : kUnicodeSpaces) {
                if (value.starts_with(space)) {
                    return space.size();
                }
            }
            return 0;
        }

        [[nodiscard]] constexpr std::size_t trailingSpace(const std::string_view value) noexcept {
            if (static_cast<unsigned char>(value.back()) < 0x80) {
                return isAsciiSpace(value.back()) ? 1 : 0;
            }
            if (value.ends_with(kIdeographicSpace)) {
                return kIdeographicSpace.size();
            }
            for (const auto space : kUnicodeSpaces) {
                if (value.ends_with(space)) {
                    return space.size();
                }
            }
            return 0;
        }

    } // namespace whitespace_detail

    // Trims what Python str.strip() trims (ASCII and Unicode whitespace) from UTF-8 text that
    // spreadsheets carry; a multi-byte sequence is never split.
    [[nodiscard]] inline std::string trimWhitespace(std::string_view value) {
        while (!value.empty()) {
            const auto size = whitespace_detail::leadingSpace(value);
            if (size == 0) {
                break;
            }
            value.remove_prefix(size);
        }
        while (!value.empty()) {
            const auto size = whitespace_detail::trailingSpace(value);
            if (size == 0) {
                break;
            }
            value.remove_suffix(size);
        }
        return std::string{value};
    }

} // namespace ssa::domain
