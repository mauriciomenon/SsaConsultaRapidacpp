#pragma once

#include "domain/SsaTypes.h"
#include "query/SearchParser.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace ssa::query {

    class TextFilterSqlCompiler final {
      public:
        void appendGeneralSearch(std::ostringstream& where, std::vector<std::string>& bindings,
                                 const SearchExpression& expression, bool& hasCondition) const;

        void appendColumnSearch(std::ostringstream& where, std::vector<std::string>& bindings,
                                const domain::SsaFilterExpression& filter,
                                bool& hasCondition) const;

        void appendQuickSector(std::ostringstream& where, std::vector<std::string>& bindings,
                               const domain::SsaFilterExpression& filter, bool& hasCondition) const;

        void appendStatusFilters(std::ostringstream& where, std::vector<std::string>& bindings,
                                 const domain::SsaFilterExpression& filter,
                                 bool& hasCondition) const;
    };

} // namespace ssa::query
