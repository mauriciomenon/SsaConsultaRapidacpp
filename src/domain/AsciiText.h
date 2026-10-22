#pragma once

// Locale-independent ASCII classification for UTF-8 text.
//
// <cctype> follows LC_CTYPE, and Qt switches LC_CTYPE to a UTF-8 locale (always on macOS).
// There toupper/tolower/isspace/isalpha also act on bytes >= 0x80 one byte at a time:
// tolower(0xC3) returns 0xE3 and isspace(0xA0) is true, which corrupts multi-byte sequences.
// Codes, headers, identifiers and numbers in this project are ASCII, so compare them here.

namespace ssa::domain::ascii {

    [[nodiscard]] constexpr bool isSpace(const char value) noexcept {
        return value == ' ' || (value >= '\t' && value <= '\r');
    }

    [[nodiscard]] constexpr bool isDigit(const char value) noexcept {
        return value >= '0' && value <= '9';
    }

    [[nodiscard]] constexpr bool isAlpha(const char value) noexcept {
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
    }

    [[nodiscard]] constexpr bool isAlnum(const char value) noexcept {
        return isAlpha(value) || isDigit(value);
    }

    [[nodiscard]] constexpr char toUpper(const char value) noexcept {
        return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
    }

    [[nodiscard]] constexpr char toLower(const char value) noexcept {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

} // namespace ssa::domain::ascii
