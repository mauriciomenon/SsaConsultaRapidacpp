#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"
#include "query/SqlQueryText.h"

#include <sstream>
#include <utility>

namespace ssa::query {

    namespace {

        std::vector<std::string> selectColumns(const ssa::domain::SsaPageRequest& request) {
            if (!request.visibleColumns.empty()) {
                return request.visibleColumns;
            }
            return ssa::domain::ColumnCatalog::defaultVisibleKeys();
        }

        ssa::domain::SsaFilterExpression
        filterFromRequest(const ssa::domain::SsaPageRequest& request,
                          const SearchExpression& expression) {
            ssa::domain::SsaFilterExpression filter;
            filter.generalTerms = expression.requiredTerms;
            SearchParser parser;
            for (const auto& [key, value] : request.columnFilters) {
                filter.columnTerms.emplace(key, parser.parseTerms(value));
            }
            if (!request.quickSector.empty()) {
                filter.quickSector = request.quickSector;
            }
            filter.excludeScaSesSte = request.excludeScaSesSte;
            filter.advanced = request.advancedFilters;
            return filter;
        }

        std::string orderByClause(const domain::SsaPageRequest& request) {
            std::ostringstream order;
            order << quoteColumnIdentifier(request.sort.columnKey)
                  << (request.sort.ascending ? " ASC" : " DESC");
            if (request.sort.statusLast) {
                order << ", CASE WHEN UPPER(COALESCE("
                      << quoteColumnIdentifier(
                             std::string{domain::ColumnCatalog::statusColumnKey()})
                      << ", '')) = '" << uppercaseCopy(domain::ColumnCatalog::statusLastSortCode())
                      << "' THEN 1 ELSE 0 END ASC";
            }
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

    SqlQuery SqlQueryBuilder::buildRows(const domain::SsaPageRequest& request) const {
        const auto expression = parser_.parse(request.searchText);
        const auto where =
            predicateBuilder_.build(expression, filterFromRequest(request, expression));
        return buildSelectQuery(request, tableName_, where.sql, where.bindings);
    }

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request) const {
        const auto expression = parser_.parse(request.searchText);
        const auto where =
            predicateBuilder_.build(expression, filterFromRequest(request, expression));

        SqlQuery page = buildSelectQuery(request, tableName_, where.sql, where.bindings);
        page.sql += " LIMIT ? OFFSET ?";
        page.bindings.push_back(std::to_string(request.pageSize));
        page.bindings.push_back(std::to_string(request.pageIndex * request.pageSize));

        SqlQuery count{"SELECT COUNT(*) FROM " + quoteTableIdentifier(tableName_) +
                           (where.sql.empty() ? "" : " WHERE " + where.sql),
                       where.bindings};
        return {std::move(page), std::move(count)};
    }

    SqlRecordQuery SqlQueryBuilder::buildRecordBySsaNumber(const domain::SsaNumber& number) const {
        return {SqlQuery{"SELECT * FROM " + quoteTableIdentifier(tableName_) + " WHERE " +
                             quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) +
                             " = ? ORDER BY rowid ASC LIMIT 1",
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
        sql << "WHERE ";
        if (!where.sql.empty()) {
            sql << where.sql << " AND ";
        }
        sql << column << " IS NOT NULL AND TRIM(COALESCE(" << column << ", '')) <> ''";
        sql << " ORDER BY " << column << " ASC LIMIT ?";
        where.bindings.push_back(std::to_string(request.limit));
        return {sql.str(), where.bindings};
    }

} // namespace ssa::query
