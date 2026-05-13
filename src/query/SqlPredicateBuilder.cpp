#include "query/SqlPredicateBuilder.h"

#include "domain/ColumnCatalog.h"
#include "domain/SafePattern.h"
#include "query/SqlQueryText.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace ssa::query {

    namespace {

        using SearchTerm = domain::FilterTerm;

        struct CompiledSearchTerm {
            std::string pattern;
            bool matchAll{false};
        };

        struct ValidatedSafePattern {
            std::string raw;
            std::string upper;
        };

        void validateSafePattern(const std::string& pattern) {
            if (pattern.find('?') != std::string::npos || domain::safePatternUnsupported(pattern)) {
                throw std::invalid_argument("safe pattern filter is unsupported");
            }
        }

        ValidatedSafePattern validatedSafePattern(const std::string& pattern,
                                                  const std::string& upperPattern) {
            validateSafePattern(pattern);
            return {pattern, upperPattern};
        }

        void appendLikeLiteral(std::string& result, const char ch) {
            if (ch == '%' || ch == '_' || ch == '\\') {
                result.push_back('\\');
            }
            result.push_back(ch);
        }

        void appendLikeWildcard(std::string& result) {
            result.push_back('_');
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
                    appendLikeWildcard(result);
                } else {
                    appendLikeLiteral(result, ch);
                }
            }
            return result;
        }

        std::string patternFor(const SearchTerm& term, const std::string& upperText) {
            if (term.mode == domain::MatchMode::SafePattern) {
                return safePatternLikePattern(validatedSafePattern(term.text, upperText));
            }
            const std::string escapedText = escapedLikeLiteral(upperText);
            if (term.mode == domain::MatchMode::StartsWith) {
                return escapedText + "%";
            }
            if (term.mode == domain::MatchMode::EndsWith) {
                return "%" + escapedText;
            }
            if (term.mode == domain::MatchMode::Equals) {
                return upperText;
            }
            return "%" + escapedText + "%";
        }

        std::string comparatorFor(const SearchTerm& term, const std::string& placeholder = "?") {
            if (term.mode == domain::MatchMode::Equals) {
                return " = " + placeholder + " COLLATE NOCASE ";
            }
            return " LIKE " + placeholder + " COLLATE NOCASE ESCAPE '\\' ";
        }

        CompiledSearchTerm compileSearchTerm(const SearchTerm& term) {
            if (term.mode == domain::MatchMode::SafePattern && term.text.empty()) {
                return {{}, true};
            }
            const std::string upperText = uppercaseCopy(term.text);
            return {patternFor(term, upperText), false};
        }

        std::string textValueExpression(const std::string& key) {
            return quoteColumnIdentifier(key);
        }

        std::string numericValueExpression(const std::string& key) {
            return "CAST(COALESCE(" + quoteColumnIdentifier(key) + ", 0) AS INTEGER)";
        }

        std::vector<std::string> uppercaseExcludedStatusCodes() {
            std::vector<std::string> result;
            const auto excludedCodes = domain::ColumnCatalog::excludedStatusCodes();
            result.reserve(excludedCodes.size());
            for (const auto code : excludedCodes) {
                result.push_back(uppercaseCopy(code));
            }
            return result;
        }

        const std::vector<std::string>& excludedStatusBindings() {
            static const std::vector<std::string> codes = uppercaseExcludedStatusCodes();
            return codes;
        }

        void appendAndSeparator(std::ostringstream& where, const bool hasCondition) {
            where << (hasCondition ? " AND " : "");
        }

        void appendValuePredicateForColumn(std::ostringstream& where, const std::string& key,
                                           const SearchTerm& term,
                                           const std::string& placeholder = "?") {
            where << textValueExpression(key);
            where << comparatorFor(term, placeholder);
        }

        void appendCompiledTermForColumn(std::ostringstream& where,
                                         std::vector<std::string>& bindings,
                                         const std::string& column, const SearchTerm& term,
                                         const CompiledSearchTerm& compiled) {
            appendValuePredicateForColumn(where, column, term);
            bindings.push_back(compiled.pattern);
        }

        void appendGeneralSearchTerm(std::ostringstream& where, std::vector<std::string>& bindings,
                                     const std::vector<std::string>& columns,
                                     const SearchTerm& term, bool& hasCondition) {
            const CompiledSearchTerm compiled = compileSearchTerm(term);
            if (compiled.matchAll) {
                return;
            }
            appendAndSeparator(where, hasCondition);
            if (term.negated) {
                where << "NOT ";
            }
            where << "(";
            for (std::size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) {
                    where << " OR ";
                }
                appendCompiledTermForColumn(where, bindings, columns[i], term, compiled);
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
                const CompiledSearchTerm compiled = compileSearchTerm(term);
                appendCompiledTermForColumn(group, groupBindings, key, term, compiled);
                hasPredicate = true;
            }
            if (!hasPredicate) {
                return;
            }
            appendAndSeparator(where, hasCondition);
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
                SearchTerm positive = term;
                positive.negated = false;
                const CompiledSearchTerm compiled = compileSearchTerm(positive);
                appendCompiledTermForColumn(group, groupBindings, key, positive, compiled);
                hasPredicate = true;
            }
            if (!hasPredicate) {
                return;
            }
            appendAndSeparator(where, hasCondition);
            where << "NOT (" << group.str() << ")";
            bindings.insert(bindings.end(), groupBindings.begin(), groupBindings.end());
            hasCondition = true;
        }

        void appendColumnTerms(std::ostringstream& where, std::vector<std::string>& bindings,
                               const std::string& key, const std::vector<SearchTerm>& terms,
                               bool& hasCondition) {
            appendInclusiveColumnTerms(where, bindings, key, terms, hasCondition);
            appendExclusiveColumnTerms(where, bindings, key, terms, hasCondition);
        }

        void appendStatusExclusion(std::ostringstream& where, std::vector<std::string>& bindings,
                                   bool& hasCondition) {
            appendAndSeparator(where, hasCondition);
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

        void appendAdvancedWeekFilter(std::ostringstream& where, std::vector<std::string>& bindings,
                                      const domain::AdvancedFilterSpec& advanced,
                                      bool& hasCondition) {
            if (!advanced.year.has_value() && !advanced.week.has_value()) {
                return;
            }
            if (!domain::ColumnCatalog::contains(advanced.weekColumnKey)) {
                throw std::invalid_argument("unknown week column: " + advanced.weekColumnKey);
            }
            const auto numericColumn = numericValueExpression(advanced.weekColumnKey);
            appendAndSeparator(where, hasCondition);
            if (const auto exact = advanced.exactYearWeek(); exact.has_value()) {
                where << numericColumn << " = ?";
                bindings.push_back(std::to_string(*exact));
            } else if (advanced.year.has_value()) {
                const auto startWeek = advanced.yearStartWeek();
                const auto endWeek = advanced.yearEndWeek();
                if (!startWeek.has_value() || !endWeek.has_value()) {
                    throw std::invalid_argument("invalid advanced year filter");
                }
                where << numericColumn << " BETWEEN ? AND ?";
                bindings.push_back(std::to_string(startWeek.value()));
                bindings.push_back(std::to_string(endWeek.value()));
            } else {
                where << "(" << numericColumn << " % 100) = ?";
                bindings.push_back(std::to_string(*advanced.week));
            }
            hasCondition = true;
        }

        void appendDerivationFilter(std::ostringstream& where,
                                    const domain::AdvancedFilterSpec& advanced,
                                    bool& hasCondition) {
            if (advanced.derivationMode == domain::DerivationFilterMode::All) {
                return;
            }
            const auto column =
                quoteColumnIdentifier(std::string{domain::ColumnCatalog::derivationColumnKey()});
            appendAndSeparator(where, hasCondition);
            if (advanced.derivationMode == domain::DerivationFilterMode::RootOnly) {
                where << "(" << column << " IS NULL OR " << column << " = '')";
            } else if (advanced.derivationMode == domain::DerivationFilterMode::DerivedOnly) {
                where << "(" << column << " IS NOT NULL AND " << column << " <> '')";
            } else {
                throw std::invalid_argument("unknown derivation filter mode");
            }
            hasCondition = true;
        }

        void appendReprogrammingFilter(std::ostringstream& where,
                                       const domain::AdvancedFilterSpec& advanced,
                                       bool& hasCondition) {
            if (!advanced.onlyReprogrammed) {
                return;
            }
            const auto columns = domain::ColumnCatalog::reprogrammingColumnKeys();
            appendAndSeparator(where, hasCondition);
            where << "(";
            for (std::size_t index = 0; index < columns.size(); ++index) {
                if (index > 0) {
                    where << " OR ";
                }
                where << numericValueExpression(std::string{columns[index]}) << " > 0";
            }
            where << ")";
            hasCondition = true;
        }

        void appendAdvancedFilters(std::ostringstream& where, std::vector<std::string>& bindings,
                                   const domain::AdvancedFilterSpec& advanced, bool& hasCondition) {
            appendAdvancedWeekFilter(where, bindings, advanced, hasCondition);
            appendDerivationFilter(where, advanced, hasCondition);
            appendReprogrammingFilter(where, advanced, hasCondition);
        }

        void appendGeneralSearch(std::ostringstream& where, std::vector<std::string>& bindings,
                                 const SearchExpression& expression, bool& hasCondition) {
            const auto generalSearchColumns = domain::ColumnCatalog::generalSearchKeys();
            for (const auto& term : expression.requiredTerms) {
                appendGeneralSearchTerm(where, bindings, generalSearchColumns, term, hasCondition);
            }
        }

        void appendColumnSearch(std::ostringstream& where, std::vector<std::string>& bindings,
                                const domain::SsaFilterExpression& filter, bool& hasCondition) {
            for (const auto& [key, terms] : filter.columnTerms) {
                appendColumnTerms(where, bindings, key, terms, hasCondition);
            }
        }

        void appendQuickSector(std::ostringstream& where, std::vector<std::string>& bindings,
                               const domain::SsaFilterExpression& filter, bool& hasCondition) {
            if (!filter.quickSector.has_value() || filter.quickSector->empty()) {
                return;
            }
            SearchTerm term;
            term.text = *filter.quickSector;
            appendColumnTerms(where, bindings,
                              std::string{domain::ColumnCatalog::executorColumnKey()}, {term},
                              hasCondition);
        }

        void appendStatusFilters(std::ostringstream& where, std::vector<std::string>& bindings,
                                 const domain::SsaFilterExpression& filter, bool& hasCondition) {
            if (filter.excludeScaSesSte) {
                appendStatusExclusion(where, bindings, hasCondition);
            }
        }

    } // namespace

    SqlWhereClause SqlPredicateBuilder::build(const SearchExpression& expression,
                                              const domain::SsaFilterExpression& filter) const {
        SqlWhereClause clause;
        std::ostringstream where;
        bool hasCondition = false;

        appendGeneralSearch(where, clause.bindings, expression, hasCondition);
        appendColumnSearch(where, clause.bindings, filter, hasCondition);
        appendQuickSector(where, clause.bindings, filter, hasCondition);
        appendStatusFilters(where, clause.bindings, filter, hasCondition);
        appendAdvancedFilters(where, clause.bindings, filter.advanced, hasCondition);
        clause.sql = where.str();
        return clause;
    }

} // namespace ssa::query
