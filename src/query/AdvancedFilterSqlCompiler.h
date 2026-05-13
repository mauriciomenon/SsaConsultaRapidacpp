#pragma once

#include "domain/SsaTypes.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace ssa::query {

    class AdvancedFilterSqlCompiler final {
      public:
        void appendAdvancedFilters(std::ostringstream& where, std::vector<std::string>& bindings,
                                   const domain::AdvancedFilterSpec& advanced,
                                   bool& hasCondition) const;
    };

} // namespace ssa::query
