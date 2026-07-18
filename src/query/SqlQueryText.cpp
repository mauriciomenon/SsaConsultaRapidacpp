#include "query/SqlQueryText.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <cctype>
#include <ranges>
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
        const auto valid = !name.empty() && std::ranges::all_of(name, [](const char ch) {
            const auto value = static_cast<unsigned char>(ch);
            return std::isalnum(value) != 0 || ch == '_';
        });
        if (!valid) {
            throw std::invalid_argument("invalid SQL table identifier: " + name);
        }
        return "\"" + name + "\"";
    }

    std::string uppercaseCopy(const std::string_view value) {
        std::string upper;
        upper.resize(value.size());
        std::ranges::transform(value, upper.begin(), [](const char ch) {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        });
        return upper;
    }

    std::string statusLastSortExpression() {
        const auto statusColumn =
            quoteColumnIdentifier(std::string{domain::ColumnCatalog::statusColumnKey()});
        const auto statusCode = uppercaseCopy(domain::ColumnCatalog::statusLastSortCode());
        return "CASE WHEN UPPER(COALESCE(" + statusColumn + ", '')) <> '" + statusCode +
               "' THEN 0 ELSE 1 END";
    }

    void appendSqlAndSeparator(std::ostringstream& stream, const bool hasCondition) {
        stream << (hasCondition ? " AND " : "");
    }

} // namespace ssa::query
