#include "query/SearchParser.h"

#include "domain/WhitespaceTrim.h"

namespace ssa::query {

    namespace {

        domain::FilterTerm parseTerm(const std::string_view chunk) {
            domain::FilterTerm term;
            term.text = domain::trimWhitespace(chunk);

            if (!term.text.empty() && term.text.front() == '!') {
                term.negated = true;
                term.text = domain::trimWhitespace(std::string_view(term.text).substr(1));
            }

            if (term.text.empty()) {
                return term;
            }

            const char mode = term.text.front();
            if (mode == '^') {
                term.mode = domain::MatchMode::StartsWith;
                term.text = domain::trimWhitespace(std::string_view(term.text).substr(1));
            } else if (mode == '$') {
                term.mode = domain::MatchMode::EndsWith;
                term.text = domain::trimWhitespace(std::string_view(term.text).substr(1));
            } else if (mode == '=' || mode == '~') {
                // The '~' prefix is the explicit safe-pattern mode in the GUI contract.
                term.mode =
                    mode == '=' ? domain::MatchMode::Equals : domain::MatchMode::SafePattern;
                term.text = domain::trimWhitespace(std::string_view(term.text).substr(1));
            } else if (term.text.back() == '$') {
                term.mode = domain::MatchMode::EndsWith;
                term.text = domain::trimWhitespace(
                    std::string_view(term.text).substr(0, term.text.size() - 1));
            }

            return term;
        }

    } // namespace

    SearchExpression SearchParser::parse(const std::string_view input) const {
        SearchExpression expression;
        expression.requiredTerms = parseTerms(input);
        return expression;
    }

    std::vector<domain::FilterTerm> SearchParser::parseTerms(const std::string_view input) const {
        std::vector<domain::FilterTerm> terms;
        std::size_t start = 0;

        while (start <= input.size()) {
            const std::size_t comma = input.find(',', start);
            const auto end = comma == std::string_view::npos ? input.size() : comma;
            auto term = parseTerm(input.substr(start, end - start));
            if (!term.text.empty()) {
                terms.push_back(std::move(term));
            }
            if (comma == std::string_view::npos) {
                break;
            }
            start = comma + 1;
        }

        return terms;
    }

} // namespace ssa::query
