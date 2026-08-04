#include "query/SqlQueryBuilder.h"

#include "domain/ColumnCatalog.h"
#include "domain/ColumnValuePriorityPolicy.h"
#include "query/ActivityAnalyticsSqlBuilder.h"
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

        std::string derivedCountJoinSql(const std::string& tableName,
                                        const bool useDerivedCountSummary) {
            const auto table = quoteTableIdentifier(tableName);
            const auto derivationColumn =
                quoteColumnIdentifier(std::string{domain::ColumnCatalog::derivationColumnKey()});
            const auto parentNumber =
                table + "." + quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey});
            const auto normalizedDerivation = "TRIM(COALESCE(" + derivationColumn + ", ''))";
            const auto parentAliasColumn =
                std::string{"\""} + std::string{kDerivedCountsParentColumn} + "\"";
            if (useDerivedCountSummary) {
                return " LEFT JOIN " + quoteTableIdentifier(tableName + "_derived_counts") +
                       " AS " + quoteTableIdentifier(std::string{kDerivedCountsAlias}) + " ON " +
                       quoteTableIdentifier(std::string{kDerivedCountsAlias}) + "." +
                       parentAliasColumn + " = TRIM(COALESCE(" + parentNumber + ", ''))";
            }
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

        std::string priorityCaseSql(const std::string& expression) {
            std::ostringstream sql;
            sql << "CASE";
            for (std::size_t index = 0; index < domain::kOrderedPriorityValues.size(); ++index) {
                sql << " WHEN "
                    << asciiCodeSegmentCondition(expression, domain::kOrderedPriorityValues[index])
                    << " THEN " << index;
            }
            sql << " ELSE 100 END";
            return sql.str();
        }

        std::string distinctValuesOrderSql(const std::string& column,
                                           const std::string_view columnKey,
                                           const bool numericColumn) {
            std::ostringstream sql;
            if (numericColumn) {
                sql << " ORDER BY CAST(" << column << " AS INTEGER) ASC";
            } else if (domain::usesPriorityValueOrder(columnKey)) {
                const auto executor =
                    "TRIM(COALESCE(" +
                    quoteColumnIdentifier(std::string{domain::ColumnCatalog::executorColumnKey()}) +
                    ", ''))";
                const auto issuer =
                    "TRIM(COALESCE(" + quoteColumnIdentifier("setor_emissor") + ", ''))";
                sql << " ORDER BY " << priorityCaseSql(column) << " ASC, MIN("
                    << priorityCaseSql(executor) << ") ASC, MIN(" << priorityCaseSql(issuer)
                    << ") ASC";
            } else {
                sql << " ORDER BY " << column << " COLLATE NOCASE ASC, " << column << " ASC";
                return sql.str();
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
                order << statusLastSortExpression() << " ASC, ";
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

    const std::string& SqlQueryBuilder::rawTableName() const noexcept {
        return tableName_;
    }

    std::string SqlQueryBuilder::derivedCountSummaryTableName() const {
        return tableName_ + "_derived_counts";
    }

    SqlQuery buildSelectQuery(const domain::SsaPageRequest& request, const std::string& tableName,
                              const std::string& where,
                              const std::vector<std::string>& whereBindings,
                              const bool useDerivedCountSummary) {
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
            select << derivedCountJoinSql(tableName, useDerivedCountSummary);
        }
        if (!where.empty()) {
            select << " WHERE " << where;
        }
        select << " ORDER BY " << orderByClause(request);
        return {select.str(), whereBindings};
    }

    SqlQuery buildPagedSelectQuery(const domain::SsaPageRequest& request,
                                   const std::string& tableName, const std::string& where,
                                   const std::vector<std::string>& whereBindings,
                                   const bool useDerivedCountSummary) {
        auto query =
            buildSelectQuery(request, tableName, where, whereBindings, useDerivedCountSummary);
        if (request.pageSize == 0) {
            return query;
        }
        if (request.pageIndex > (std::numeric_limits<std::size_t>::max() / request.pageSize)) {
            throw std::overflow_error("page offset exceeds supported range");
        }
        query.sql += " LIMIT ? OFFSET ?";
        query.bindings.push_back(std::to_string(request.pageSize));
        query.bindings.push_back(std::to_string(request.pageIndex * request.pageSize));
        return query;
    }

    SqlQuery SqlQueryBuilder::buildRows(const domain::SsaPageRequest& request) const {
        return buildRows(request, false);
    }

    SqlQuery SqlQueryBuilder::buildRows(const domain::SsaPageRequest& request,
                                        const bool useDerivedCountSummary) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);
        return buildPagedSelectQuery(request, tableName_, where.sql, where.bindings,
                                     useDerivedCountSummary);
    }

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request) const {
        return build(request, false);
    }

    SqlPageQueries SqlQueryBuilder::build(const domain::SsaPageRequest& request,
                                          const bool useDerivedCountSummary) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);

        SqlQuery page = buildPagedSelectQuery(request, tableName_, where.sql, where.bindings,
                                              useDerivedCountSummary);

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

    SqlQuery SqlQueryBuilder::buildExecutadasReport(const domain::SsaPageRequest& request,
                                                    const bool byDivision) const {
        const auto where = whereClauseFromRequest(request, parser_, predicateBuilder_);
        const int first = request.advancedFilters.executionWeekStart.value_or(190001);
        const int last = request.advancedFilters.executionWeekEnd.value_or(299952);
        const domain::AnalyticsRequest analyticsRequest{
            .metric = domain::AnalyticsMetric::Executed,
            .period = {{first / domain::kYearWeekMultiplier, first % domain::kYearWeekMultiplier},
                       {last / domain::kYearWeekMultiplier, last % domain::kYearWeekMultiplier}},
            .grain = domain::TimeGrain::IsoWeek,
            .breakdown = byDivision ? domain::Breakdown::DivisionPerson
                                    : domain::Breakdown::DivisionSectorPerson,
            .personRole = domain::PersonRole::Executor,
        };
        const auto analytics =
            ActivityAnalyticsSqlBuilder{tableName_}.buildSeries(analyticsRequest, where);
        const std::string group = byDivision ? "\"division\"" : "\"sector\"";
        // bucket_key is already compact YYYYWW (e.g. 202503) for IsoWeek grain.
        const std::string week = "\"bucket_key\"";
        const std::string person =
            "CASE WHEN \"person\" = 'Nao atribuido' THEN '-' ELSE \"person\" END";

        std::ostringstream sql;
        sql << "SELECT " << group << " AS \"group\", " << week << " AS \"week\", " << person
            << " AS \"person\", \"count\" AS \"count\" FROM (" << analytics.sql
            << ") AS \"analytics_rows\" WHERE " << group << " <> 'Nao atribuido' ORDER BY " << group
            << " COLLATE NOCASE ASC, " << group << " ASC, " << week << " ASC, " << person
            << " COLLATE NOCASE ASC, " << person << " ASC";
        return {sql.str(), analytics.bindings};
    }

    SqlRecordQuery SqlQueryBuilder::buildRecordBySsaNumber(const domain::SsaNumber& number) const {
        return {SqlQuery{"SELECT * FROM " + quoteTableIdentifier(tableName_) + " WHERE " +
                             quoteColumnIdentifier(std::string{domain::kSsaNumberColumnKey}) +
                             " = ? ORDER BY " + quoteColumnIdentifier("id") + " ASC LIMIT 1",
                         {std::string{number.value()}}}};
    }

    SqlQuery SqlQueryBuilder::buildDirectDerivations(const domain::SsaNumber& number) const {
        return {"SELECT " + quoteColumnIdentifier("numero_ssa") + ", " +
                    quoteColumnIdentifier("situacao") + " FROM " +
                    quoteTableIdentifier(tableName_) + " WHERE " +
                    quoteColumnIdentifier("derivada_de") + " = ? AND " +
                    quoteColumnIdentifier("numero_ssa") + " IS NOT NULL ORDER BY " +
                    quoteColumnIdentifier("numero_ssa"),
                {std::string{number.value()}}};
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
        auto filter = request.filter;
        SearchParser parser;
        appendColumnFilters(filter, request.columnFilters, parser);
        appendAdvancedTextFilters(filter, request.filter.advanced.textFilters, parser);
        auto where = predicateBuilder_.build(expression, filter);
        std::ostringstream sql;
        sql << "SELECT " << projection << " FROM " << quoteTableIdentifier(tableName_) << " ";
        sql << "WHERE " << distinctValuesWhereSql(projection, where);
        sql << " GROUP BY " << projection;
        sql << distinctValuesOrderSql(projection, request.columnKey,
                                      isNumericDistinctColumn(request.columnKey))
            << " LIMIT ?";
        auto bindings = std::move(where.bindings);
        bindings.push_back(std::to_string(request.limit));
        return {sql.str(), std::move(bindings)};
    }

    SqlQuery SqlQueryBuilder::buildMaxValueLength(const std::string_view columnKey) const {
        const auto* column = domain::ColumnCatalog::find(columnKey);
        if (column == nullptr || domain::ColumnCatalog::isDerivedCountColumn(columnKey)) {
            throw std::invalid_argument("maximum value length is not supported for column: " +
                                        std::string{columnKey});
        }
        const auto identifier = quoteColumnIdentifier(std::string{columnKey});
        return {"SELECT MAX(LENGTH(TRIM(COALESCE(" + identifier + ", '')))) FROM " +
                    quoteTableIdentifier(tableName_),
                {}};
    }

} // namespace ssa::query
