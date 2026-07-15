#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ssa::domain {

    enum class TextFilterOperator : std::uint8_t {
        Equals,
        Different,
    };

    enum class TextFilterUiMode : std::uint8_t {
        Equals,
        Different,
        Mixed,
    };

    struct TextFilterToken {
        TextFilterOperator filterOperator{TextFilterOperator::Equals};
        std::string value;
    };

    struct TextFilterTokenSet {
        std::vector<TextFilterToken> ordered;
        std::unordered_map<std::string, std::size_t> indexByValue;
    };

    [[nodiscard]] std::optional<TextFilterOperator>
    textFilterOperatorFromMode(std::string_view mode);
    [[nodiscard]] std::string textFilterOperatorMode(TextFilterOperator filterOperator);
    [[nodiscard]] TextFilterUiMode textFilterUiModeFromName(std::string_view mode);
    [[nodiscard]] std::string textFilterUiModeName(TextFilterUiMode mode);
    [[nodiscard]] std::string makeTextFilterToken(std::string_view value,
                                                  TextFilterOperator filterOperator);
    [[nodiscard]] TextFilterTokenSet parseTextFilterTokens(std::string_view expression);
    [[nodiscard]] TextFilterTokenSet makeTextFilterTokenSet(const std::vector<std::string>& values,
                                                            TextFilterOperator filterOperator);
    bool addTextFilterValue(TextFilterTokenSet& tokens, std::string_view value,
                            TextFilterOperator filterOperator);
    [[nodiscard]] bool sameTextFilterTokens(const TextFilterTokenSet& lhs,
                                            const TextFilterTokenSet& rhs);
    [[nodiscard]] std::string joinTextFilterTokens(const TextFilterTokenSet& tokens);
    [[nodiscard]] TextFilterUiMode textFilterUiModeForTokens(const TextFilterTokenSet& tokens,
                                                             TextFilterUiMode emptyFallback);
    [[nodiscard]] TextFilterUiMode textFilterUiModeForTokens(const TextFilterTokenSet& tokens);
    [[nodiscard]] std::string textFilterUiModeNameForTokens(const TextFilterTokenSet& tokens);

} // namespace ssa::domain
