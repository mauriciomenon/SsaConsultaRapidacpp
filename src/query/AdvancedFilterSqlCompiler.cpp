#include "query/AdvancedFilterSqlCompiler.h"

#include "domain/ColumnCatalog.h"
#include "query/SqlQueryText.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ssa::query {

    namespace {

        std::string numericValueExpression(const std::string& key) {
            auto column = quoteColumnIdentifier(key);
            // Integer columns (semana_*, reprogramacoes) are stored natively as
            // INTEGER by the import writer, so filtering on the raw column keeps the
            // predicate sargable and lets the index (created at import time) be used.
            // Defensive CAST/GLOB wrapping is reserved for text-affinity columns.
            if (const auto* def = domain::ColumnCatalog::find(key)) {
                if (def->type == domain::ColumnType::Integer) {
                    return column;
                }
            }
            const auto text = "TRIM(COALESCE(" + column + ", ''))";
            return "CAST(CASE WHEN " + text + " <> '' AND " + text + " NOT GLOB '*[^0-9]*' THEN " +
                   text + " ELSE NULL END AS INTEGER)";
        }

        std::vector<std::string> buildReprogrammingNumericExpressions() {
            std::vector<std::string> expressions;
            const auto columns = domain::ColumnCatalog::reprogrammingColumnKeys();
            expressions.reserve(columns.size());
            for (const auto column : columns) {
                expressions.push_back(numericValueExpression(std::string{column}));
            }
            return expressions;
        }

        const std::vector<std::string>& reprogrammingNumericExpressions() {
            static const std::vector<std::string> expressions =
                buildReprogrammingNumericExpressions();
            return expressions;
        }

        void appendAdvancedWeekFilter(std::ostringstream& where, std::vector<std::string>& bindings,
                                      const domain::AdvancedFilterSpec& advanced,
                                      bool& hasCondition) {
            if (!advanced.year.has_value() && !advanced.week.has_value()) {
                return;
            }
            if (advanced.year.has_value() && !domain::isValidFilterYear(*advanced.year)) {
                throw std::invalid_argument("invalid advanced year filter");
            }
            if (advanced.week.has_value() && !domain::isValidIsoWeek(*advanced.week)) {
                throw std::invalid_argument("invalid advanced week filter");
            }
            if (advanced.year.has_value() && advanced.week.has_value() &&
                !domain::isValidIsoYearWeek(*advanced.year, *advanced.week)) {
                throw std::invalid_argument("invalid advanced week filter");
            }
            if (!domain::ColumnCatalog::contains(advanced.weekColumnKey)) {
                throw std::invalid_argument("unknown week column: " + advanced.weekColumnKey);
            }
            const auto numericColumn = numericValueExpression(advanced.weekColumnKey);
            appendSqlAndSeparator(where, hasCondition);
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
            } else if (advanced.week.has_value()) {
                where << "(" << numericColumn << " % 100) = ?";
                bindings.push_back(std::to_string(*advanced.week));
            } else {
                throw std::invalid_argument("invalid advanced week filter");
            }
            hasCondition = true;
        }

        void appendYearFromWeekFilter(std::ostringstream& where, std::vector<std::string>& bindings,
                                      const std::string& columnKey, const std::optional<int> year,
                                      bool& hasCondition) {
            if (!year.has_value()) {
                return;
            }
            const auto startWeek = domain::composeIsoYearWeek(*year, domain::kFirstIsoWeek);
            const auto lastWeek = domain::lastIsoWeekOfYear(*year);
            const auto endWeek =
                lastWeek.has_value() ? domain::composeIsoYearWeek(*year, *lastWeek) : std::nullopt;
            if (!startWeek.has_value() || !endWeek.has_value()) {
                throw std::invalid_argument("invalid year filter");
            }
            appendSqlAndSeparator(where, hasCondition);
            where << numericValueExpression(columnKey) << " BETWEEN ? AND ?";
            bindings.push_back(std::to_string(*startWeek));
            bindings.push_back(std::to_string(*endWeek));
            hasCondition = true;
        }

        void appendWeekRangeFilter(std::ostringstream& where, std::vector<std::string>& bindings,
                                   const std::string& columnKey, const std::optional<int> startWeek,
                                   const std::optional<int> endWeek, bool& hasCondition) {
            if (!startWeek.has_value() && !endWeek.has_value()) {
                return;
            }
            if ((startWeek.has_value() && !domain::isValidIsoYearWeek(*startWeek)) ||
                (endWeek.has_value() && !domain::isValidIsoYearWeek(*endWeek))) {
                throw std::invalid_argument("invalid year-week range filter");
            }
            appendSqlAndSeparator(where, hasCondition);
            const auto numericColumn = numericValueExpression(columnKey);
            if (startWeek.has_value() && endWeek.has_value()) {
                where << numericColumn << " BETWEEN ? AND ?";
                bindings.push_back(std::to_string(*startWeek));
                bindings.push_back(std::to_string(*endWeek));
            } else if (startWeek.has_value()) {
                where << numericColumn << " >= ?";
                bindings.push_back(std::to_string(*startWeek));
            } else {
                where << numericColumn << " <= ?";
                bindings.push_back(std::to_string(*endWeek));
            }
            hasCondition = true;
        }

        void appendReprogrammingValueFilter(std::ostringstream& where,
                                            std::vector<std::string>& bindings,
                                            const domain::AdvancedFilterSpec& advanced,
                                            bool& hasCondition) {
            if (advanced.reprogrammingValues.empty()) {
                return;
            }
            appendSqlAndSeparator(where, hasCondition);
            const auto column = numericValueExpression(
                std::string{domain::ColumnCatalog::primaryReprogrammingColumnKey()});
            if (!advanced.reprogrammingValues.empty() &&
                advanced.reprogrammingComparison == domain::NumericComparisonMode::Equals) {
                where << column << " IN (";
                for (std::size_t index = 0; index < advanced.reprogrammingValues.size(); ++index) {
                    if (index > 0) {
                        where << ", ";
                    }
                    where << "?";
                    bindings.push_back(std::to_string(advanced.reprogrammingValues[index]));
                }
                where << ")";
            } else {
                const auto [minIt, maxIt] =
                    std::ranges::minmax_element(advanced.reprogrammingValues);
                const int value =
                    advanced.reprogrammingComparison == domain::NumericComparisonMode::LessOrEqual
                        ? *maxIt
                        : *minIt;
                where << column << " "
                      << domain::numericComparisonOperator(advanced.reprogrammingComparison)
                      << " ?";
                bindings.push_back(std::to_string(value));
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
            appendSqlAndSeparator(where, hasCondition);
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
            const auto& expressions = reprogrammingNumericExpressions();
            if (expressions.empty()) {
                return;
            }
            appendSqlAndSeparator(where, hasCondition);
            where << "(";
            for (std::size_t index = 0; index < expressions.size(); ++index) {
                if (index > 0) {
                    where << " OR ";
                }
                where << expressions[index] << " > 0";
            }
            where << ")";
            hasCondition = true;
        }

    } // namespace

    void AdvancedFilterSqlCompiler::appendAdvancedFilters(
        std::ostringstream& where, std::vector<std::string>& bindings,
        const domain::AdvancedFilterSpec& advanced, bool& hasCondition) const {
        appendAdvancedWeekFilter(where, bindings, advanced, hasCondition);
        appendYearFromWeekFilter(where, bindings,
                                 std::string{domain::ColumnCatalog::issueWeekColumnKey()},
                                 advanced.issueYear, hasCondition);
        appendYearFromWeekFilter(where, bindings,
                                 std::string{domain::ColumnCatalog::executionWeekColumnKey()},
                                 advanced.executionYear, hasCondition);
        appendReprogrammingValueFilter(where, bindings, advanced, hasCondition);
        appendWeekRangeFilter(where, bindings,
                              std::string{domain::ColumnCatalog::issueWeekColumnKey()},
                              advanced.issueWeekStart, advanced.issueWeekEnd, hasCondition);
        appendWeekRangeFilter(where, bindings,
                              std::string{domain::ColumnCatalog::executionWeekColumnKey()},
                              advanced.executionWeekStart, advanced.executionWeekEnd, hasCondition);
        appendDerivationFilter(where, advanced, hasCondition);
        appendReprogrammingFilter(where, advanced, hasCondition);
    }

} // namespace ssa::query
