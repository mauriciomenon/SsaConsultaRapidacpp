#include "query/SqlQueryText.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace ssa::query {

    std::string quoteColumnIdentifier(const std::string& key) {
        if (!ssa::domain::ColumnCatalog::contains(key)) {
            throw std::invalid_argument("unknown column: " + key);
        }
        return "\"" + key + "\"";
    }

    std::string quoteTableIdentifier(const std::string& name) {
        return "\"" + name + "\"";
    }

    std::string uppercaseCopy(const std::string_view value) {
        std::string upper;
        upper.resize(value.size());
        std::ranges::transform(value, upper.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return upper;
    }

    void appendSqlAndSeparator(std::ostringstream& stream, const bool hasCondition) {
        stream << (hasCondition ? " AND " : "");
    }

} // namespace ssa::query
