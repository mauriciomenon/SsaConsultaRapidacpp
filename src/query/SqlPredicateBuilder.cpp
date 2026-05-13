#include "query/SqlPredicateBuilder.h"

#include "query/AdvancedFilterSqlCompiler.h"
#include "query/TextFilterSqlCompiler.h"

#include <sstream>

namespace ssa::query {

    SqlWhereClause SqlPredicateBuilder::build(const SearchExpression& expression,
                                              const domain::SsaFilterExpression& filter) const {
        SqlWhereClause clause;
        std::ostringstream where;
        bool hasCondition = false;
        const TextFilterSqlCompiler textFilters;
        const AdvancedFilterSqlCompiler advancedFilters;

        textFilters.appendGeneralSearch(where, clause.bindings, expression, hasCondition);
        textFilters.appendColumnSearch(where, clause.bindings, filter, hasCondition);
        textFilters.appendQuickSector(where, clause.bindings, filter, hasCondition);
        textFilters.appendStatusFilters(where, clause.bindings, filter, hasCondition);
        advancedFilters.appendAdvancedFilters(where, clause.bindings, filter.advanced,
                                              hasCondition);

        clause.sql = where.str();
        return clause;
    }

} // namespace ssa::query
