#pragma once

#include "domain/SsaTypes.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::query {

    class AdvancedFilterSqlCompiler final {
      public:
        // tableName must outlive the compiler; parent/child filters read it in a subquery.
        explicit AdvancedFilterSqlCompiler(std::string_view tableName) : tableName_(tableName) {}

        void appendAdvancedFilters(std::ostringstream& where, std::vector<std::string>& bindings,
                                   const domain::AdvancedFilterSpec& advanced,
                                   bool& hasCondition) const;

      private:
        std::string_view tableName_;
    };

} // namespace ssa::query
