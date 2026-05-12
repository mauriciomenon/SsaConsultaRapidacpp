#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace ssa::query {

    using SearchTerm = domain::FilterTerm;

    namespace {

        std::string quoteIdentifier(const std::string& key) {
            if (!ssa::domain::ColumnCatalog::contains(key)) {
                throw std::invalid_argument("unknown column: " + key);
            }
            return "\"" + key + "\"";
        }

        std::vector<std::string> selectColumns(const ssa::domain::SsaPageRequest& request) {
            if (!request.visibleColumns.empty()) {
                return request.visibleColumns;
            }
            return ssa::domain::ColumnCatalog::defaultVisibleKeys();
        }

        std::string patternFor(const SearchTerm& term) {
            std::string text = term.text;
            std::ranges::transform(text, text.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            if (term.mode == domain::MatchMode::StartsWith) {
                return text + "%";
            }
            if (term.mode == domain::MatchMode::EndsWith) {
                return "%" + text;
            }
            if (term.mode == domain::MatchMode::Equals || term.mode == domain::MatchMode::Regex) {
                return text;
            }
            return "%" + text + "%";
        }

        std::string comparatorFor(const SearchTerm& term) {
            if (term.mode == domain::MatchMode::Equals) {
                return " = ? ";
            }
            if (term.mode == domain::MatchMode::Regex) {
                return " REGEXP ? ";
            }
            return " LIKE ? ";
        }

        void appendGeneralSearchTerm(std::ostringstream& where, std::vector<std::string>& bindings,
                                     const SearchTerm& term, bool& hasWhere) {
            const auto columns = ssa::domain::ColumnCatalog::generalSearchKeys();
            where << (hasWhere ? " AND " : " WHERE ");
            if (term.negated) {
                where << "NOT ";
            }
            where << "(";
            for (std::size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) {
                    where << " OR ";
                }
                where << "UPPER(COALESCE(" << quoteIdentifier(columns[i]) << ", ''))"
                      << comparatorFor(term);
                bindings.push_back(patternFor(term));
            }
            where << ")";
            hasWhere = true;
        }

        void appendTermForColumn(std::ostringstream& where, std::vector<std::string>& bindings,
                                 const std::string& key, const SearchTerm& term) {
            where << "UPPER(COALESCE(" << quoteIdentifier(key) << ", ''))" << comparatorFor(term);
            bindings.push_back(patternFor(term));
        }

        void appendColumnTerms(std::ostringstream& where, std::vector<std::string>& bindings,
                               const std::string& key, const std::vector<SearchTerm>& terms,
                               bool& hasWhere) {
            std::vector<SearchTerm> includes;
            std::vector<SearchTerm> excludes;
            for (const auto& term : terms) {
                if (term.text.empty()) {
                    continue;
                }
                if (term.negated) {
                    excludes.push_back(term);
                } else {
                    includes.push_back(term);
                }
            }
            if (includes.empty() && excludes.empty()) {
                return;
            }

            if (!includes.empty()) {
                where << (hasWhere ? " AND " : " WHERE ");
                where << "(";
                for (std::size_t index = 0; index < includes.size(); ++index) {
                    if (index > 0) {
                        where << " OR ";
                    }
                    appendTermForColumn(where, bindings, key, includes[index]);
                }
                where << ")";
                hasWhere = true;
            }

            for (const auto& term : excludes) {
                where << (hasWhere ? " AND " : " WHERE ");
                where << "NOT (";
                SearchTerm positive = term;
                positive.negated = false;
                appendTermForColumn(where, bindings, key, positive);
                where << ")";
                hasWhere = true;
            }
        }

        std::string buildWhere(const ssa::domain::SsaPageRequest& request,
                               const SearchExpression& expression,
                               std::vector<std::string>& bindings) {
            std::ostringstream where;
            bool hasWhere = false;

            for (const auto& term : expression.requiredTerms) {
                appendGeneralSearchTerm(where, bindings, term, hasWhere);
            }

            for (const auto& [key, value] : request.columnFilters) {
                SearchParser parser;
                appendColumnTerms(where, bindings, key, parser.parseTerms(value), hasWhere);
            }

            if (!request.quickSector.empty()) {
                SearchTerm term;
                term.text = request.quickSector;
                appendColumnTerms(where, bindings, "setor_executor", {term}, hasWhere);
            }

            if (request.excludeScaSesSte) {
                where << (hasWhere ? " AND " : " WHERE ");
                where << "UPPER(COALESCE(\"situacao\", '')) NOT IN ('SCA', 'SES', 'STE')";
            }

            return where.str();
        }

        std::string orderByClause(const domain::SsaPageRequest& request) {
            std::ostringstream order;
            if (request.sort.statusLast && request.sort.columnKey == "numero_ssa") {
                order << "CASE WHEN UPPER(COALESCE(\"situacao\", '')) = 'STE' THEN 1 ELSE 0 END "
                         "ASC, ";
            }
            order << quoteIdentifier(request.sort.columnKey)
                  << (request.sort.ascending ? " ASC" : " DESC");
            return order.str();
        }

    } // namespace

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request) const {
        if (request.pageSize == 0) {
            throw std::invalid_argument("page size must be greater than zero");
        }

        const auto expression = parser_.parse(request.searchText);
        std::vector<std::string> whereBindings;
        const std::string where = buildWhere(request, expression, whereBindings);

        const auto columns = selectColumns(request);
        std::ostringstream select;
        select << "SELECT ";
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) {
                select << ", ";
            }
            select << quoteIdentifier(columns[i]);
        }
        select << " FROM \"ssa_table\"" << where;
        select << " ORDER BY " << orderByClause(request);
        select << " LIMIT ? OFFSET ?";

        SqlQuery page{select.str(), whereBindings};
        page.bindings.push_back(std::to_string(request.pageSize));
        page.bindings.push_back(std::to_string(request.pageIndex * request.pageSize));

        SqlQuery count{"SELECT COUNT(*) FROM \"ssa_table\"" + where, whereBindings};
        return {std::move(page), std::move(count)};
    }

    SqlRecordQuery SqlQueryBuilder::buildRecordById(const domain::SsaId& id) const {
        return {
            SqlQuery{"SELECT * FROM \"ssa_table\" WHERE \"numero_ssa\" = ? LIMIT 1", {id.value()}}};
    }

    SqlQuery
    SqlQueryBuilder::buildDistinctValues(const domain::DistinctValuesRequest& request) const {
        const std::string column = quoteIdentifier(request.columnKey);
        std::ostringstream sql;
        sql << "SELECT DISTINCT " << column << " FROM \"ssa_table\" WHERE " << column
            << " IS NOT NULL AND TRIM(COALESCE(" << column << ", '')) <> ''";
        sql << " ORDER BY " << column << " ASC LIMIT ?";
        return {sql.str(), {std::to_string(request.limit)}};
    }

} // namespace ssa::query
