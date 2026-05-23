#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"
#include "query/SqlQueryText.h"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ssa::query {

    namespace {

        std::vector<std::string> selectColumns(const ssa::domain::SsaPageRequest& request) {
            if (!request.visibleColumns.empty()) {
                return request.visibleColumns;
            }
            return ssa::domain::ColumnCatalog::defaultVisibleKeys();
        }

        void appendColumnFilters(ssa::domain::SsaFilterExpression& filter,
                                 const std::map<std::string, std::string>& filters,
                                 SearchParser& parser) {
            for (const auto& [key, value] : filters) {
                filter.columnTerms[key] = parser.parseTerms(value);
            }
        }

        void appendAdvancedTextFilters(ssa::domain::SsaFilterExpression& filter,
                                       const std::map<std::string, std::string>& filters,
                                       SearchParser& parser) {
            for (const auto& [key, value] : filters) {
                auto terms = parser.parseTerms(value);
                auto& columnTerms = filter.columnTerms[key];
                columnTerms.insert(columnTerms.end(), terms.begin(), terms.end());
            }
        }

        ssa::domain::SsaFilterExpression
        filterFromRequest(const ssa::domain::SsaPageRequest& request,
                          const SearchExpression& expression) {
            ssa::domain::SsaFilterExpression filter;
            filter.generalTerms = expression.requiredTerms;
            SearchParser parser;
            appendColumnFilters(filter, request.columnFilters, parser);
            appendAdvancedTextFilters(filter, request.advancedFilters.textFilters, parser);
            if (!request.quickSector.empty()) {
                filter.quickSector = request.quickSector;
            }
            filter.excludeScaSesSte = request.excludeScaSesSte;
            filter.advanced = request.advancedFilters;
            return filter;
        }

        SqlWhereClause whereClauseFromRequest(const domain::SsaPageRequest& request,
                                              const SearchParser& parser,
                                              const SqlPredicateBuilder& predicateBuilder) {
            const auto expression = parser.parse(request.searchText);
            return predicateBuilder.build(expression, filterFromRequest(request, expression));
        }

        std::string distinctValuesWhereSql(const std::string& column, const SqlWhereClause& where) {
            std::ostringstream sql;
            sql << column << " IS NOT NULL AND TRIM(COALESCE(" << column << ", '')) <> ''";
            if (!where.sql.empty()) {
                sql << " AND " << where.sql;
            }
            return sql.str();
        }

        std::string singleQuotedSqlLiteral(const std::string& value) {
            std::string literal{"'"};
            for (const char ch : value) {
                if (ch == '\'') {
                    literal += "''";
                } else {
                    literal.push_back(ch);
                }
            }
            literal.push_back('\'');
            return literal;
        }

        std::string orderByClause(const domain::SsaPageRequest& request) {
            std::ostringstream order;
            if (request.sort.statusLast) {
                static const std::string kStatusLastSortCode =
                    uppercaseCopy(domain::ColumnCatalog::statusLastSortCode());
                order << "CASE WHEN UPPER(COALESCE("
                      << quoteColumnIdentifier(
                             std::string{domain::ColumnCatalog::statusColumnKey()})
                      << ", '')) <> " << singleQuotedSqlLiteral(kStatusLastSortCode)
                      << " THEN 0 ELSE 1 END ASC, ";
            }
            order << quoteColumnIdentifier(request.sort.columnKey)
                  << (request.sort.ascending ? " ASC" : " DESC");
            return order.str();
        }

    } // namespace

    SqlQueryBuilder::SqlQueryBuilder(std::string tableName) : tableName_(std::move(tableName)) {}

    SqlQuery buildSelectQuery(const domain::SsaPageRequest& request, const std::string& tableName,
                              const std::string& where,
                              const std::vector<std::string>& whereBindings) {
        const auto columns = selectColumns(request);
        std::ostringstream select;
        select << "SELECT ";
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) {
                select << ", ";
            }
            select << quoteColumnIdentifier(columns[i]);
        }
        select << " FROM " << quoteTableIdentifier(tableName);
        if (!where.empty()) {
            select << " WHERE " << where;
        }
        select << " ORDER BY " << orderByClause(request);
        return {select.str(), whereBindings};
    }

    SqlQuery buildPagedSelectQuery(const domain::SsaPageRequest& request,
                                   const std::string& tableName, const std::string& where,
                                   const std::vector<std::string>& whereBindings) {
        auto query = buildSelectQuery(request, tableName, where, whereBindings);
        if (request.pageSize == 0) {
            return query;
        }
        if (request.pageSize != 0 &&
            request.pageIndex > (std::numeric_limits<std::size_t>::max() / request.pageSize)) {
            throw std::overflow_error("page offset exceeds supported range");
        }
        query.sql += " LIMIT ? OFFSET ?";
        query.bindings.push_back(std::to_string(request.pageSize));
        query.bindings.push_back(std::to_string(request.pageIndex * request.pageSize));
        return query;
    }

    SqlQuery SqlQueryBuilder::buildRows(const domain::SsaPageRequest& request) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);
        return buildSelectQuery(request, tableName_, where.sql, where.bindings);
    }

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);

        SqlQuery page = buildPagedSelectQuery(request, tableName_, where.sql, where.bindings);

        SqlQuery count{"SELECT COUNT(*) FROM " + quoteTableIdentifier(tableName_) +
                           (where.sql.empty() ? "" : " WHERE " + where.sql),
                       where.bindings};
        return {std::move(page), std::move(count)};
    }

    SqlRecordQuery SqlQueryBuilder::buildRecordBySsaNumber(const domain::SsaNumber& number) const {
        return {SqlQuery{"SELECT * FROM " + quoteTableIdentifier(tableName_) + " WHERE " +
                             quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) +
                             " = ? ORDER BY " + quoteColumnIdentifier("id") + " ASC LIMIT 1",
                         {std::string{number.value()}}}};
    }

    SqlQuery
    SqlQueryBuilder::buildDistinctValues(const domain::DistinctValuesRequest& request) const {
        const std::string column = quoteColumnIdentifier(request.columnKey);
        SearchExpression expression;
        expression.requiredTerms = request.filter.generalTerms;
        auto where = predicateBuilder_.build(expression, request.filter);
        std::ostringstream sql;
        sql << "SELECT DISTINCT " << column << " FROM " << quoteTableIdentifier(tableName_) << " ";
        sql << "WHERE " << distinctValuesWhereSql(column, where);
        sql << " ORDER BY " << column << " ASC LIMIT ?";
        auto bindings = std::move(where.bindings);
        bindings.push_back(std::to_string(request.limit));
        return {sql.str(), std::move(bindings)};
    }

} // namespace ssa::query
