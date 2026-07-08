#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"
#include "query/SqlQueryText.h"

#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ssa::query {

    namespace {

        std::vector<std::string> selectColumns(const ssa::domain::SsaPageRequest& request) {
            if (!request.visibleColumns.empty()) {
                return ssa::domain::ColumnCatalog::visibleKeysOrDefault(request.visibleColumns);
            }
            return ssa::domain::ColumnCatalog::visibleKeysOrDefault({});
        }

        std::string qualifiedColumnIdentifier(const std::string& qualifier,
                                              const std::string& columnKey) {
            return quoteTableIdentifier(qualifier) + "." + quoteColumnIdentifier(columnKey);
        }

        constexpr std::string_view kDerivedCountsAlias = "derived_counts";
        constexpr std::string_view kDerivedCountsParentColumn = "parent_ssa";

        bool usesDerivedCountColumn(const std::vector<std::string>& columns,
                                    const domain::SsaPageRequest& request) {
            return std::ranges::any_of(columns, domain::ColumnCatalog::isDerivedCountColumn) ||
                   domain::ColumnCatalog::isDerivedCountColumn(request.sort.columnKey);
        }

        std::string derivedCountProjection() {
            return "COALESCE(" +
                   qualifiedColumnIdentifier(
                       std::string{kDerivedCountsAlias},
                       std::string{domain::ColumnCatalog::derivedCountColumnKey()}) +
                   ", 0)";
        }

        std::string derivedCountJoinSql(const std::string& tableName) {
            const auto table = quoteTableIdentifier(tableName);
            const auto derivationColumn =
                quoteColumnIdentifier(std::string{domain::ColumnCatalog::derivationColumnKey()});
            const auto parentNumber =
                table + "." + quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey});
            const auto normalizedDerivation = "TRIM(COALESCE(" + derivationColumn + ", ''))";
            const auto parentAliasColumn =
                std::string{"\""} + std::string{kDerivedCountsParentColumn} + "\"";
            return " LEFT JOIN (SELECT " + normalizedDerivation + " AS " + parentAliasColumn +
                   ", COUNT(*) AS " +
                   quoteColumnIdentifier(
                       std::string{domain::ColumnCatalog::derivedCountColumnKey()}) +
                   " FROM " + table + " WHERE " + normalizedDerivation + " <> '' GROUP BY " +
                   normalizedDerivation + ") AS " +
                   quoteTableIdentifier(std::string{kDerivedCountsAlias}) + " ON " +
                   quoteTableIdentifier(std::string{kDerivedCountsAlias}) + "." +
                   parentAliasColumn + " = TRIM(COALESCE(" + parentNumber + ", ''))";
        }

        std::string selectExpressionForColumn(const std::string& columnKey) {
            if (domain::ColumnCatalog::isDerivedCountColumn(columnKey)) {
                return derivedCountProjection() + " AS " + quoteColumnIdentifier(columnKey);
            }
            return quoteColumnIdentifier(columnKey);
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

        std::string distinctValuesWhereSql(const std::string& projection,
                                           const SqlWhereClause& where) {
            std::ostringstream sql;
            sql << projection << " <> ''";
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

        std::string asciiCodeSegmentCondition(const std::string& expression,
                                              const std::string_view code) {
            const auto codeLiteral = singleQuotedSqlLiteral(std::string{code});
            const auto nextChar =
                "SUBSTR(UPPER(" + expression + "), " + std::to_string(code.size() + 1) + ", 1)";
            return "UPPER(" + expression + ") = " + codeLiteral + " OR (UPPER(" + expression +
                   ") LIKE " + singleQuotedSqlLiteral(std::string{code} + "%") + " AND NOT (" +
                   nextChar + " BETWEEN 'A' AND 'Z') AND NOT (" + nextChar +
                   " BETWEEN '0' AND '9'))";
        }

        bool isNumericDistinctColumn(const std::string_view columnKey) {
            const auto* column = domain::ColumnCatalog::find(columnKey);
            return column != nullptr && column->type == domain::ColumnType::Integer;
        }

        constexpr std::array<std::string_view, 8> kOrderedPriorityValues{
            "IEE3", "IEE1", "IEE2", "IEE4", "MEL1", "MEL2", "MEL3", "MEL4"};

        std::string priorityCaseSql(const std::string& expression) {
            std::ostringstream sql;
            sql << "CASE";
            for (std::size_t index = 0; index < kOrderedPriorityValues.size(); ++index) {
                sql << " WHEN "
                    << asciiCodeSegmentCondition(expression, kOrderedPriorityValues[index])
                    << " THEN " << index;
            }
            sql << " ELSE 100 END";
            return sql.str();
        }

        std::string distinctValuesOrderSql(const std::string& column, const bool numericColumn) {
            std::ostringstream sql;
            if (numericColumn) {
                sql << " ORDER BY CAST(" << column << " AS INTEGER) ASC";
            } else {
                const auto executor =
                    "TRIM(COALESCE(" +
                    quoteColumnIdentifier(std::string{domain::ColumnCatalog::executorColumnKey()}) +
                    ", ''))";
                const auto issuer =
                    "TRIM(COALESCE(" + quoteColumnIdentifier("setor_emissor") + ", ''))";
                sql << " ORDER BY " << priorityCaseSql(column) << " ASC, MIN("
                    << priorityCaseSql(executor) << ") ASC, MIN(" << priorityCaseSql(issuer)
                    << ") ASC";
            }
            sql << ", " << column << " COLLATE NOCASE ASC, " << column << " ASC";
            return sql.str();
        }

        std::string orderByExpression(const std::string& sortKey) {
            if (domain::ColumnCatalog::isDerivedCountColumn(sortKey)) {
                return derivedCountProjection();
            }
            return quoteColumnIdentifier(sortKey);
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
            const auto& sortKey = request.sort.columnKey.empty()
                                      ? std::string{domain::kSsaNumberColumnKey}
                                      : request.sort.columnKey;
            const bool sortAscending =
                request.sort.columnKey.empty() ? false : request.sort.ascending;
            if (!domain::ColumnCatalog::contains(sortKey)) {
                throw std::invalid_argument("unknown sort column: " + sortKey);
            }
            order << orderByExpression(sortKey) << (sortAscending ? " ASC" : " DESC");
            return order.str();
        }

    } // namespace

    SqlQueryBuilder::SqlQueryBuilder(std::string tableName) : tableName_(std::move(tableName)) {}

    std::string SqlQueryBuilder::tableName() const {
        return quoteTableIdentifier(tableName_);
    }

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
            select << selectExpressionForColumn(columns[i]);
        }
        select << " FROM " << quoteTableIdentifier(tableName);
        if (usesDerivedCountColumn(columns, request)) {
            select << derivedCountJoinSql(tableName);
        }
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
        return buildPagedSelectQuery(request, tableName_, where.sql, where.bindings);
    }

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);

        SqlQuery page = buildPagedSelectQuery(request, tableName_, where.sql, where.bindings);

        SqlQuery count{"SELECT COUNT(*) FROM " + quoteTableIdentifier(tableName_) +
                           (where.sql.empty() ? "" : " WHERE " + where.sql),
                       where.bindings};
        return {std::move(page), std::move(count)};
    }

    SqlQuery SqlQueryBuilder::buildCount(const domain::SsaPageRequest& request) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);
        return {"SELECT COUNT(*) FROM " + quoteTableIdentifier(tableName_) +
                    (where.sql.empty() ? "" : " WHERE " + where.sql),
                where.bindings};
    }

    SqlRecordQuery SqlQueryBuilder::buildRecordBySsaNumber(const domain::SsaNumber& number) const {
        return {SqlQuery{"SELECT * FROM " + quoteTableIdentifier(tableName_) + " WHERE " +
                             quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) +
                             " = ? ORDER BY " + quoteColumnIdentifier("id") + " ASC LIMIT 1",
                         {std::string{number.value()}}}};
    }

    SqlQuery
    SqlQueryBuilder::buildDistinctValues(const domain::DistinctValuesRequest& request) const {
        if (domain::ColumnCatalog::isDerivedCountColumn(request.columnKey)) {
            throw std::invalid_argument("distinct values are not supported for derived count");
        }
        const std::string column = quoteColumnIdentifier(request.columnKey);
        // Project TRIM(COALESCE(column,'')) so the result is already normalized and
        // the C++ fetcher does not need a second trim/empty-check pass. Grouping and
        // ordering use the same expression so whitespace variants collapse together.
        const std::string projection = "TRIM(COALESCE(" + column + ", ''))";
        SearchExpression expression;
        expression.requiredTerms = request.filter.generalTerms;
        auto where = predicateBuilder_.build(expression, request.filter);
        std::ostringstream sql;
        sql << "SELECT " << projection << " FROM " << quoteTableIdentifier(tableName_) << " ";
        sql << "WHERE " << distinctValuesWhereSql(projection, where);
        sql << " GROUP BY " << projection;
        sql << distinctValuesOrderSql(projection, isNumericDistinctColumn(request.columnKey))
            << " LIMIT ?";
        auto bindings = std::move(where.bindings);
        bindings.push_back(std::to_string(request.limit));
        return {sql.str(), std::move(bindings)};
    }

} // namespace ssa::query
