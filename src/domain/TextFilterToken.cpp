#include "domain/TextFilterToken.h"

#include "domain/WhitespaceTrim.h"

#include <utility>

namespace ssa::domain {

    namespace {

        constexpr char kDifferentPrefix = '!';
        constexpr char kEqualsPrefix = '=';

        std::string_view tokenPrefix(const TextFilterOperator filterOperator) {
            return filterOperator == TextFilterOperator::Different ? "!=" : "=";
        }

        TextFilterToken parseToken(const std::string_view rawToken) {
            auto token = trimWhitespace(rawToken);
            if (token.empty()) {
                return {};
            }
            auto filterOperator = TextFilterOperator::Equals;
            if (token.front() == kDifferentPrefix) {
                filterOperator = TextFilterOperator::Different;
                token.erase(token.begin());
                if (!token.empty() && token.front() == kEqualsPrefix) {
                    token.erase(token.begin());
                }
            } else if (token.front() == kEqualsPrefix) {
                token.erase(token.begin());
            }
            return TextFilterToken{filterOperator, trimWhitespace(token)};
        }

        std::string serializeToken(const TextFilterToken& token) {
            if (token.value.empty()) {
                return {};
            }
            std::string serialized;
            serialized.reserve(token.value.size() + tokenPrefix(token.filterOperator).size());
            serialized.append(tokenPrefix(token.filterOperator));
            serialized.append(token.value);
            return serialized;
        }

        bool appendOrReplaceByValue(TextFilterTokenSet& tokens, TextFilterToken token) {
            token.value = trimWhitespace(token.value);
            if (token.value.empty()) {
                return false;
            }
            if (const auto existing = tokens.indexByValue.find(token.value);
                existing != tokens.indexByValue.end()) {
                if (tokens.ordered[existing->second].filterOperator == token.filterOperator) {
                    return false;
                }
                tokens.ordered[existing->second].filterOperator = token.filterOperator;
                return true;
            }
            tokens.indexByValue[token.value] = tokens.ordered.size();
            tokens.ordered.push_back(std::move(token));
            return true;
        }

    } // namespace

    std::optional<TextFilterOperator> textFilterOperatorFromMode(const std::string_view mode) {
        if (mode == "different") {
            return TextFilterOperator::Different;
        }
        if (mode == "equals") {
            return TextFilterOperator::Equals;
        }
        return std::nullopt;
    }

    std::string textFilterOperatorMode(const TextFilterOperator filterOperator) {
        return filterOperator == TextFilterOperator::Different ? "different" : "equals";
    }

    TextFilterUiMode textFilterUiModeFromName(const std::string_view mode) {
        if (mode == "different") {
            return TextFilterUiMode::Different;
        }
        return mode == "mixed" ? TextFilterUiMode::Mixed : TextFilterUiMode::Equals;
    }

    std::string textFilterUiModeName(const TextFilterUiMode mode) {
        if (mode == TextFilterUiMode::Different) {
            return "different";
        }
        return mode == TextFilterUiMode::Mixed ? "mixed" : "equals";
    }

    std::string makeTextFilterToken(const std::string_view value,
                                    const TextFilterOperator filterOperator) {
        return serializeToken(TextFilterToken{filterOperator, trimWhitespace(value)});
    }

    TextFilterTokenSet parseTextFilterTokens(const std::string_view expression) {
        TextFilterTokenSet tokens;
        std::size_t start = 0;
        while (start < expression.size()) {
            const auto end = expression.find(',', start);
            const auto part = expression.substr(
                start, end == std::string_view::npos ? std::string_view::npos : end - start);
            appendOrReplaceByValue(tokens, parseToken(part));
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        return tokens;
    }

    TextFilterTokenSet makeTextFilterTokenSet(const std::vector<std::string>& values,
                                              const TextFilterOperator filterOperator) {
        TextFilterTokenSet tokens;
        for (const auto& value : values) {
            appendOrReplaceByValue(tokens, TextFilterToken{filterOperator, trimWhitespace(value)});
        }
        return tokens;
    }

    bool addTextFilterValue(TextFilterTokenSet& tokens, const std::string_view value,
                            const TextFilterOperator filterOperator) {
        return appendOrReplaceByValue(tokens,
                                      TextFilterToken{filterOperator, trimWhitespace(value)});
    }

    bool sameTextFilterTokens(const TextFilterTokenSet& lhs, const TextFilterTokenSet& rhs) {
        if (lhs.ordered.size() != rhs.ordered.size()) {
            return false;
        }
        for (std::size_t index = 0; index < lhs.ordered.size(); ++index) {
            if (lhs.ordered[index].filterOperator != rhs.ordered[index].filterOperator ||
                lhs.ordered[index].value != rhs.ordered[index].value) {
                return false;
            }
        }
        return true;
    }

    std::string joinTextFilterTokens(const TextFilterTokenSet& tokens) {
        std::string expression;
        for (const auto& token : tokens.ordered) {
            const auto serialized = serializeToken(token);
            if (serialized.empty()) {
                continue;
            }
            if (!expression.empty()) {
                expression.push_back(',');
            }
            expression.append(serialized);
        }
        return expression;
    }

    TextFilterUiMode textFilterUiModeForTokens(const TextFilterTokenSet& tokens,
                                               const TextFilterUiMode emptyFallback) {
        if (tokens.ordered.empty()) {
            return emptyFallback;
        }
        bool hasEquals = false;
        bool hasDifferent = false;
        for (const auto& token : tokens.ordered) {
            hasDifferent = hasDifferent || token.filterOperator == TextFilterOperator::Different;
            hasEquals = hasEquals || token.filterOperator == TextFilterOperator::Equals;
        }
        if (hasEquals && hasDifferent) {
            return TextFilterUiMode::Mixed;
        }
        return hasDifferent ? TextFilterUiMode::Different : TextFilterUiMode::Equals;
    }

    TextFilterUiMode textFilterUiModeForTokens(const TextFilterTokenSet& tokens) {
        return textFilterUiModeForTokens(tokens, TextFilterUiMode::Equals);
    }

    std::string textFilterUiModeNameForTokens(const TextFilterTokenSet& tokens) {
        return textFilterUiModeName(textFilterUiModeForTokens(tokens));
    }

} // namespace ssa::domain
