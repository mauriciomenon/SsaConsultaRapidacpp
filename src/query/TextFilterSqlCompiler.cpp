#include "query/TextFilterSqlCompiler.h"

#include "domain/ColumnCatalog.h"
#include "domain/SafePattern.h"
#include "query/SqlQueryText.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace ssa::query {

    namespace {

        using SearchTerm = domain::FilterTerm;

        struct CompiledSearchTerm {
            std::string pattern;
            std::string comparator;
            bool matchAll{false};
        };

        struct ValidatedSafePattern {
            std::string upper;
        };

        void validateSafePattern(const std::string& pattern) {
            if (pattern.find('?') != std::string::npos || domain::safePatternUnsupported(pattern)) {
                throw std::invalid_argument("safe pattern filter is unsupported");
            }
        }

        ValidatedSafePattern validatedSafePattern(const std::string& upperPattern) {
            validateSafePattern(upperPattern);
            return {upperPattern};
        }

        void appendLikeLiteral(std::string& result, const char ch) {
            if (ch == '%' || ch == '_' || ch == '\\') {
                result.push_back('\\');
            }
            result.push_back(ch);
        }

        std::string escapedLikeLiteral(const std::string& text) {
            std::string result;
            result.reserve(text.size());
            for (const char ch : text) {
                appendLikeLiteral(result, ch);
            }
            return result;
        }

        std::string safePatternLikePattern(const ValidatedSafePattern& pattern) {
            std::string result;
            result.reserve(pattern.upper.size());
            for (const char ch : pattern.upper) {
                if (ch == '.') {
                    result.push_back('_');
                } else {
                    appendLikeLiteral(result, ch);
                }
            }
            return result;
        }

        std::string patternFor(const domain::MatchMode mode, const std::string& upperText) {
            if (mode == domain::MatchMode::SafePattern) {
                return safePatternLikePattern(validatedSafePattern(upperText));
            }
            const std::string escapedText = escapedLikeLiteral(upperText);
            if (mode == domain::MatchMode::StartsWith) {
                return escapedText + "%";
            }
            if (mode == domain::MatchMode::EndsWith) {
                return "%" + escapedText;
            }
            if (mode == domain::MatchMode::Equals) {
                return upperText;
            }
            return "%" + escapedText + "%";
        }

        std::string comparatorFor(const domain::MatchMode mode) {
            if (mode == domain::MatchMode::Equals) {
                return " = ? COLLATE NOCASE ";
            }
            return " LIKE ? COLLATE NOCASE ESCAPE '\\' ";
        }

        CompiledSearchTerm compileSearchTerm(const std::string& text,
                                             const domain::MatchMode mode) {
            if (text.empty()) {
                return {{}, {}, true};
            }
            const std::string upperText = uppercaseCopy(text);
            return {patternFor(mode, upperText), comparatorFor(mode), false};
        }

        std::vector<std::string> uppercaseExcludedStatusCodes() {
            std::vector<std::string> result;
            const auto excludedCodes = domain::ColumnCatalog::excludedStatusCodes();
            result.reserve(excludedCodes.size());
            std::ranges::transform(excludedCodes, std::back_inserter(result), uppercaseCopy);
            return result;
        }

        const std::vector<std::string>& excludedStatusBindings() {
            static const std::vector<std::string> codes = uppercaseExcludedStatusCodes();
            return codes;
        }

        void appendCompiledTermForColumn(std::ostringstream& where,
                                         std::vector<std::string>& bindings,
                                         const std::string& column,
                                         const CompiledSearchTerm& compiled) {
            where << quoteColumnIdentifier(column);
            where << compiled.comparator;
            bindings.push_back(compiled.pattern);
        }

        void appendGeneralSearchTerm(std::ostringstream& where, std::vector<std::string>& bindings,
                                     const std::vector<std::string>& columns,
                                     const SearchTerm& term, bool& hasCondition) {
            const CompiledSearchTerm compiled = compileSearchTerm(term.text, term.mode);
            if (compiled.matchAll) {
                return;
            }
            appendSqlAndSeparator(where, hasCondition);
            if (term.negated) {
                where << "NOT ";
            }
            where << "(";
            for (std::size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) {
                    where << " OR ";
                }
                appendCompiledTermForColumn(where, bindings, columns[i], compiled);
            }
            where << ")";
            hasCondition = true;
        }

        void appendInclusiveColumnTerms(std::ostringstream& where,
                                        std::vector<std::string>& bindings, const std::string& key,
                                        const std::vector<SearchTerm>& terms, bool& hasCondition) {
            std::ostringstream group;
            std::vector<std::string> groupBindings;
            bool hasPredicate = false;
            for (const auto& term : terms) {
                if (term.text.empty() || term.negated) {
                    continue;
                }
                if (hasPredicate) {
                    group << " OR ";
                }
                const CompiledSearchTerm compiled = compileSearchTerm(term.text, term.mode);
                appendCompiledTermForColumn(group, groupBindings, key, compiled);
                hasPredicate = true;
            }
            if (!hasPredicate) {
                return;
            }
            appendSqlAndSeparator(where, hasCondition);
            where << "(" << group.str() << ")";
            bindings.insert(bindings.end(), groupBindings.begin(), groupBindings.end());
            hasCondition = true;
        }

        void appendExclusiveColumnTerms(std::ostringstream& where,
                                        std::vector<std::string>& bindings, const std::string& key,
                                        const std::vector<SearchTerm>& terms, bool& hasCondition) {
            std::ostringstream group;
            std::vector<std::string> groupBindings;
            bool hasPredicate = false;
            for (const auto& term : terms) {
                if (term.text.empty() || !term.negated) {
                    continue;
                }
                if (hasPredicate) {
                    group << " OR ";
                }
                const CompiledSearchTerm compiled = compileSearchTerm(term.text, term.mode);
                appendCompiledTermForColumn(group, groupBindings, key, compiled);
                hasPredicate = true;
            }
            if (!hasPredicate) {
                return;
            }
            appendSqlAndSeparator(where, hasCondition);
            where << "NOT (" << group.str() << ")";
            bindings.insert(bindings.end(), groupBindings.begin(), groupBindings.end());
            hasCondition = true;
        }

        void appendColumnTerms(std::ostringstream& where, std::vector<std::string>& bindings,
                               const std::string& key, const std::vector<SearchTerm>& terms,
                               bool& hasCondition) {
            if (domain::ColumnCatalog::isDerivedCountColumn(key)) {
                throw std::invalid_argument("filters are not supported for derived count");
            }
            appendInclusiveColumnTerms(where, bindings, key, terms, hasCondition);
            appendExclusiveColumnTerms(where, bindings, key, terms, hasCondition);
        }

        void appendStatusExclusion(std::ostringstream& where, std::vector<std::string>& bindings,
                                   bool& hasCondition) {
            appendSqlAndSeparator(where, hasCondition);
            where << "COALESCE("
                  << quoteColumnIdentifier(std::string{domain::ColumnCatalog::statusColumnKey()})
                  << ", '') COLLATE NOCASE NOT IN (";
            const auto& excludedCodes = excludedStatusBindings();
            for (std::size_t index = 0; index < excludedCodes.size(); ++index) {
                if (index > 0) {
                    where << ", ";
                }
                where << "?";
                bindings.push_back(excludedCodes[index]);
            }
            where << ")";
            hasCondition = true;
        }

    } // namespace

    void TextFilterSqlCompiler::appendGeneralSearch(std::ostringstream& where,
                                                    std::vector<std::string>& bindings,
                                                    const SearchExpression& expression,
                                                    bool& hasCondition) const {
        const auto generalSearchColumns = domain::ColumnCatalog::generalSearchKeys();
        for (const auto& term : expression.requiredTerms) {
            appendGeneralSearchTerm(where, bindings, generalSearchColumns, term, hasCondition);
        }
    }

    void TextFilterSqlCompiler::appendColumnSearch(std::ostringstream& where,
                                                   std::vector<std::string>& bindings,
                                                   const domain::SsaFilterExpression& filter,
                                                   bool& hasCondition) const {
        for (const auto& [key, terms] : filter.columnTerms) {
            appendColumnTerms(where, bindings, key, terms, hasCondition);
        }
    }

    void TextFilterSqlCompiler::appendQuickSector(std::ostringstream& where,
                                                  std::vector<std::string>& bindings,
                                                  const domain::SsaFilterExpression& filter,
                                                  bool& hasCondition) const {
        if (!filter.quickSector.has_value() || filter.quickSector->empty()) {
            return;
        }
        SearchTerm term;
        term.text = *filter.quickSector;
        const CompiledSearchTerm compiled = compileSearchTerm(term.text, term.mode);
        if (compiled.matchAll) {
            return;
        }
        appendSqlAndSeparator(where, hasCondition);
        appendCompiledTermForColumn(
            where, bindings, std::string{domain::ColumnCatalog::executorColumnKey()}, compiled);
        hasCondition = true;
    }

    void TextFilterSqlCompiler::appendStatusFilters(std::ostringstream& where,
                                                    std::vector<std::string>& bindings,
                                                    const domain::SsaFilterExpression& filter,
                                                    bool& hasCondition) const {
        if (filter.excludeScaSesSte) {
            appendStatusExclusion(where, bindings, hasCondition);
        }
    }

} // namespace ssa::query
