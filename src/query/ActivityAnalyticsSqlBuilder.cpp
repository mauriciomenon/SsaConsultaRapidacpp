#include "query/ActivityAnalyticsSqlBuilder.h"

#include "query/SqlQueryText.h"

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ssa::query {

    namespace {

        constexpr std::string_view kAnalyticsSnapshotTable = "activity_analytics_snapshot";
        constexpr std::string_view kAnalyticsPointTable = "activity_analytics_point";
        constexpr std::string_view kUnassigned = "Nao atribuido";

        bool isEventMetric(const domain::AnalyticsMetric metric) noexcept {
            return metric == domain::AnalyticsMetric::Registered ||
                   metric == domain::AnalyticsMetric::Executed ||
                   metric == domain::AnalyticsMetric::Issued;
        }

        std::string metricKey(const domain::AnalyticsMetric metric) {
            switch (metric) {
            case domain::AnalyticsMetric::Registered:
                return "registered";
            case domain::AnalyticsMetric::Executed:
                return "executed";
            case domain::AnalyticsMetric::PartialAttention:
                return "partial_attention";
            case domain::AnalyticsMetric::Spg:
                return "spg";
            case domain::AnalyticsMetric::Apg:
                return "apg";
            case domain::AnalyticsMetric::Apl:
                return "apl";
            case domain::AnalyticsMetric::Pending:
                return "pending";
            case domain::AnalyticsMetric::Issued:
                return "issued";
            case domain::AnalyticsMetric::PendingDeadline:
                return "pending_deadline";
            }
            throw std::invalid_argument("unknown analytics metric");
        }

        std::string personRoleKey(const domain::PersonRole role) {
            switch (role) {
            case domain::PersonRole::Requester:
                return "requester";
            case domain::PersonRole::Planner:
                return "planner";
            case domain::PersonRole::Executor:
                return "executor";
            }
            throw std::invalid_argument("unknown analytics person role");
        }

        std::string personColumn(const domain::PersonRole role) {
            switch (role) {
            case domain::PersonRole::Requester:
                return quoteColumnIdentifier("solicitante");
            case domain::PersonRole::Planner:
                return quoteColumnIdentifier("responsavel_programacao");
            case domain::PersonRole::Executor:
                return quoteColumnIdentifier("responsavel_execucao");
            }
            throw std::invalid_argument("unknown analytics person role");
        }

        std::string eventWeekColumn(const domain::AnalyticsMetric metric) {
            switch (metric) {
            case domain::AnalyticsMetric::Registered:
            case domain::AnalyticsMetric::Issued:
                return quoteColumnIdentifier("semana_cadastro");
            case domain::AnalyticsMetric::Executed:
                return quoteColumnIdentifier("semana_executada");
            default:
                throw std::invalid_argument("stock analytics has no event week column");
            }
        }

        std::string sectorColumn(const domain::AnalyticsMetric metric) {
            return metric == domain::AnalyticsMetric::Issued
                       ? quoteColumnIdentifier("setor_emissor")
                       : quoteColumnIdentifier("setor_executor");
        }

        std::string normalizedDimension(const std::string& column) {
            return "COALESCE(NULLIF(UPPER(TRIM(COALESCE(" + column + ", ''))), ''), '" +
                   std::string{kUnassigned} + "')";
        }

        std::string normalizedPerson(const std::string& column) {
            return "COALESCE(NULLIF(TRIM(COALESCE(" + column + ", '')), ''), '" +
                   std::string{kUnassigned} + "')";
        }

        std::string divisionExpression(const std::string& sector) {
            return "COALESCE(NULLIF(SUBSTR(UPPER(TRIM(COALESCE(" + sector +
                   ", ''))), 1, 3), ''), '" + std::string{kUnassigned} + "')";
        }

        std::string weekExpression(const std::string& column) {
            return canonicalIsoWeekSqlExpression(column);
        }

        std::string isoThursdayExpression(const std::string& isoYearWeek) {
            const auto year = "((" + isoYearWeek + ") / 100)";
            const auto week = "((" + isoYearWeek + ") % 100)";
            const auto januaryFourth = "printf('%04d-01-04', " + year + ")";
            const auto mondayOffset =
                "((CAST(strftime('%w', " + januaryFourth + ") AS INTEGER) + 6) % 7)";
            const auto thursdayOffset = "((" + week + " - 1) * 7 + 3 - " + mondayOffset + ")";
            return "date(" + januaryFourth + ", printf('%+d days', " + thursdayOffset + "))";
        }

        std::string validIsoWeekExpression(const std::string& isoYearWeek) {
            const auto year = "((" + isoYearWeek + ") / 100)";
            const auto week = "((" + isoYearWeek + ") % 100)";
            return "(" + year + " BETWEEN 1900 AND 2999 AND " + week +
                   " BETWEEN 1 AND 53 AND CAST(strftime('%Y', " +
                   isoThursdayExpression(isoYearWeek) + ") AS INTEGER) = " + year + ')';
        }

        std::string registrationWeekExpression() {
            const auto column = quoteColumnIdentifier("semana_cadastro");
            return canonicalIsoWeekSqlExpression(column);
        }

        std::string numberExpression() {
            return "TRIM(COALESCE(" + quoteColumnIdentifier("numero_ssa") + ", ''))";
        }

        std::string bucketExpression(const std::string& isoYearWeek,
                                     const domain::TimeGrain grain) {
            switch (grain) {
            case domain::TimeGrain::WholePeriod:
                return "''";
            case domain::TimeGrain::IsoWeek:
                return "printf('%04d-W%02d', (" + isoYearWeek + ") / 100, (" + isoYearWeek +
                       ") % 100)";
            case domain::TimeGrain::IsoReferenceMonth: {
                return "strftime('%Y-%m', " + isoThursdayExpression(isoYearWeek) + ')';
            }
            }
            throw std::invalid_argument("unknown analytics time grain");
        }

        bool usesSector(const domain::Breakdown breakdown) noexcept {
            return breakdown == domain::Breakdown::DivisionSector ||
                   breakdown == domain::Breakdown::DivisionSectorPerson;
        }

        bool usesPerson(const domain::Breakdown breakdown) noexcept {
            return breakdown == domain::Breakdown::DivisionPerson ||
                   breakdown == domain::Breakdown::DivisionSectorPerson;
        }

        std::string projectedSector(const domain::Breakdown breakdown) {
            return usesSector(breakdown) ? "\"sector\" AS \"sector\"" : "'' AS \"sector\"";
        }

        std::string projectedPerson(const domain::Breakdown breakdown) {
            return usesPerson(breakdown) ? "\"person\" AS \"person\"" : "'' AS \"person\"";
        }

        std::string groupedDimensions(const domain::Breakdown breakdown) {
            std::string dimensions{"\"bucket_key\", \"division\""};
            if (usesSector(breakdown)) {
                dimensions += ", \"sector\"";
            }
            if (usesPerson(breakdown)) {
                dimensions += ", \"person\"";
            }
            return dimensions + ", \"registration_cohort\", \"deadline_class\"";
        }

        void appendInFilter(std::ostringstream& where, bool& hasCondition,
                            const std::string_view column, const std::vector<std::string>& values,
                            std::vector<std::string>& bindings) {
            if (values.empty()) {
                return;
            }
            where << (hasCondition ? " AND " : " WHERE ") << '"' << column << "\" IN (";
            for (std::size_t index = 0; index < values.size(); ++index) {
                if (index != 0) {
                    where << ", ";
                }
                where << '?';
                bindings.push_back(values[index]);
            }
            where << ')';
            hasCondition = true;
        }

        std::string selectionWhere(const domain::AnalyticsRequest& request,
                                   std::vector<std::string>& bindings, const bool divisions,
                                   const bool sectors, const bool people) {
            std::ostringstream where;
            bool hasCondition = false;
            if (divisions) {
                appendInFilter(where, hasCondition, "division", request.divisions, bindings);
            }
            if (sectors) {
                appendInFilter(where, hasCondition, "sector", request.sectors, bindings);
            }
            if (people) {
                appendInFilter(where, hasCondition, "person", request.people, bindings);
            }
            return where.str();
        }

        std::string registrationCohortExpression() {
            return "CASE WHEN \"registration_iso_week\" IS NULL THEN 'unknown' "
                   "WHEN \"registration_iso_week\" < ? THEN 'before' "
                   "WHEN \"registration_iso_week\" <= ? THEN 'in_period' ELSE 'unknown' END";
        }

        std::string deadlineClassExpression(const domain::AnalyticsMetric metric) {
            if (metric != domain::AnalyticsMetric::PendingDeadline) {
                return "'not_applicable_or_unknown'";
            }
            return "CASE WHEN UPPER(TRIM(COALESCE(\"deadline_source_state\", ''))) = "
                   "'NAO SE APLICA' THEN 'not_applicable_or_unknown' "
                   "WHEN UPPER(TRIM(COALESCE(\"deadline_source_state\", ''))) = "
                   "'FORA DE PRAZO' OR \"deadline_offset_days\" < 0 THEN 'overdue' "
                   "WHEN \"deadline_offset_days\" BETWEEN 0 AND ? THEN 'warning' "
                   "WHEN \"deadline_offset_days\" > ? THEN 'on_time' "
                   "ELSE 'not_applicable_or_unknown' END";
        }

        void appendCohortBindings(const domain::AnalyticsRequest& request,
                                  std::vector<std::string>& bindings) {
            bindings.push_back(std::to_string(domain::toIsoYearWeek(request.period.first)));
            bindings.push_back(std::to_string(domain::toIsoYearWeek(request.period.last)));
        }

        SqlQuery buildEventSeries(const domain::AnalyticsRequest& request,
                                  const std::string& sourceTable,
                                  const SqlWhereClause& sourceFilter = {},
                                  const bool collapseRegistrationCohorts = false) {
            const auto weekColumn = eventWeekColumn(request.metric);
            const auto week = weekExpression(weekColumn);
            const auto sector = sectorColumn(request.metric);
            const auto bucket = bucketExpression("\"event_iso_week\"", request.grain);
            std::vector<std::string> bindings{
                std::to_string(domain::toIsoYearWeek(request.period.first)),
                std::to_string(domain::toIsoYearWeek(request.period.last)),
            };

            std::ostringstream sql;
            sql << "WITH event_rows AS (SELECT " << week << " AS \"event_iso_week\", "
                << divisionExpression(sector) << " AS \"division\", " << normalizedDimension(sector)
                << " AS \"sector\", " << normalizedPerson(personColumn(request.personRole))
                << " AS \"person\", " << registrationWeekExpression()
                << " AS \"registration_iso_week\", " << numberExpression()
                << " AS \"ssa_number\" FROM " << sourceTable << " WHERE " << numberExpression()
                << " <> '' AND " << weekColumn << " BETWEEN ? AND ?";
            if (!sourceFilter.sql.empty()) {
                sql << " AND (" << sourceFilter.sql << ')';
                bindings.insert(bindings.end(), sourceFilter.bindings.begin(),
                                sourceFilter.bindings.end());
            }
            sql << " AND " << week << " IS NOT NULL";
            sql << "), classified_rows AS (SELECT \"event_iso_week\", \"division\", \"sector\", "
                   "\"person\", \"registration_iso_week\", \"ssa_number\", ";
            if (collapseRegistrationCohorts) {
                sql << "'unknown'";
            } else {
                sql << registrationCohortExpression();
                appendCohortBindings(request, bindings);
            }
            sql << " AS \"registration_cohort\" FROM event_rows) ";
            sql << "SELECT " << bucket << " AS \"bucket_key\", \"division\" AS \"division\", "
                << projectedSector(request.breakdown) << ", " << projectedPerson(request.breakdown)
                << ", \"registration_cohort\" AS \"registration_cohort\", "
                << "'not_applicable_or_unknown' AS \"deadline_class\", "
                << "NULL AS \"observed_iso_week\", '' AS \"source_revision\", "
                   "COUNT(DISTINCT \"ssa_number\") AS \"count\" "
                << "FROM classified_rows";
            sql << selectionWhere(request, bindings, true, true, true);
            sql << " GROUP BY " << groupedDimensions(request.breakdown)
                << " ORDER BY \"bucket_key\", "
                   "\"division\" COLLATE NOCASE, \"sector\" COLLATE NOCASE, \"person\" COLLATE "
                   "NOCASE, \"registration_cohort\", \"deadline_class\"";
            return {sql.str(), std::move(bindings)};
        }

        std::string stockSelectionCte(const domain::AnalyticsRequest& request,
                                      const std::string& dataset,
                                      std::vector<std::string>& bindings,
                                      const bool requireComplete = true) {
            const auto first = std::to_string(domain::toIsoYearWeek(request.period.first));
            const auto last = std::to_string(domain::toIsoYearWeek(request.period.last));
            const std::string completeFilter =
                requireComplete ? " AND \"complete\" = 1" : std::string{};
            bindings.push_back(dataset);
            bindings.push_back(metricKey(request.metric));

            std::ostringstream sql;
            const auto snapshotTable = quoteTableIdentifier(std::string{kAnalyticsSnapshotTable});
            if (request.grain == domain::TimeGrain::WholePeriod) {
                bindings.push_back(last);
                sql << "eligible_snapshots AS (SELECT *, '' AS \"bucket_key\", "
                       "ROW_NUMBER() OVER (ORDER BY \"observed_iso_week\" DESC) AS \"rank\" FROM "
                    << snapshotTable
                    << " WHERE \"dataset\" = ? AND \"metric\" = ? AND "
                       "\"observed_iso_week\" <= ?"
                    << completeFilter
                    << "), "
                       "selected_snapshots AS (SELECT * FROM eligible_snapshots WHERE \"rank\" = "
                       "1)";
                return sql.str();
            }

            bindings.push_back(first);
            bindings.push_back(last);
            const auto bucket = bucketExpression("\"observed_iso_week\"", request.grain);
            if (request.grain == domain::TimeGrain::IsoWeek) {
                sql << "selected_snapshots AS (SELECT *, " << bucket << " AS \"bucket_key\" FROM "
                    << snapshotTable
                    << " WHERE \"dataset\" = ? AND \"metric\" = ? AND "
                       "\"observed_iso_week\" BETWEEN ? AND ?"
                    << completeFilter << ')';
                return sql.str();
            }

            sql << "eligible_snapshots AS (SELECT *, " << bucket
                << " AS \"bucket_key\", ROW_NUMBER() OVER (PARTITION BY " << bucket
                << " ORDER BY \"observed_iso_week\" DESC) AS \"rank\" FROM " << snapshotTable
                << " WHERE \"dataset\" = ? AND \"metric\" = ? AND "
                   "\"observed_iso_week\" BETWEEN ? AND ?"
                << completeFilter
                << "), "
                   "selected_snapshots AS (SELECT * FROM eligible_snapshots WHERE \"rank\" = 1)";
            return sql.str();
        }

        SqlQuery buildStockSeries(const domain::AnalyticsRequest& request,
                                  const std::string& dataset) {
            std::vector<std::string> bindings;
            std::ostringstream sql;
            sql << "WITH " << stockSelectionCte(request, dataset, bindings)
                << ", stock_rows AS (SELECT snapshots.\"bucket_key\", "
                   "snapshots.\"observed_iso_week\", snapshots.\"source_revision\", "
                   "points.\"division\", points.\"sector\", "
                   "points.\"person\", points.\"registration_iso_week\", "
                   "points.\"deadline_source_state\", points.\"deadline_offset_days\", "
                   "points.\"count\" FROM selected_snapshots AS snapshots JOIN "
                << quoteTableIdentifier(std::string{kAnalyticsPointTable})
                << " AS points ON points.\"dataset\" = snapshots.\"dataset\" AND "
                   "points.\"metric\" = snapshots.\"metric\" AND "
                   "points.\"observed_iso_week\" = snapshots.\"observed_iso_week\" WHERE "
                   "points.\"person_role\" = ?), classified_rows AS (SELECT *, "
                << registrationCohortExpression() << " AS \"registration_cohort\", "
                << deadlineClassExpression(request.metric)
                << " AS \"deadline_class\" FROM stock_rows) ";
            bindings.push_back(personRoleKey(request.personRole));
            appendCohortBindings(request, bindings);
            if (request.metric == domain::AnalyticsMetric::PendingDeadline) {
                if (!request.warningWindowDays.has_value()) {
                    throw std::logic_error("validated deadline analytics lacks a warning window");
                }
                const auto window = std::to_string(request.warningWindowDays.value());
                bindings.push_back(window);
                bindings.push_back(window);
            }
            sql << "SELECT \"bucket_key\" AS \"bucket_key\", \"division\" AS \"division\", "
                << projectedSector(request.breakdown) << ", " << projectedPerson(request.breakdown)
                << ", \"registration_cohort\" AS \"registration_cohort\", "
                   "\"deadline_class\" AS \"deadline_class\", "
                   "MAX(\"observed_iso_week\") AS \"observed_iso_week\", "
                   "\"source_revision\" AS \"source_revision\", "
                   "SUM(\"count\") AS \"count\" FROM classified_rows";
            sql << selectionWhere(request, bindings, true, true, true);
            sql << " GROUP BY " << groupedDimensions(request.breakdown) << ", \"source_revision\""
                << " ORDER BY \"bucket_key\", "
                   "\"division\" COLLATE NOCASE, \"sector\" COLLATE NOCASE, \"person\" COLLATE "
                   "NOCASE, \"registration_cohort\", \"deadline_class\"";
            return {sql.str(), std::move(bindings)};
        }

        struct DimensionBase final {
            std::string sql;
            std::vector<std::string> bindings;
        };

        DimensionBase buildEventDimensionBase(const domain::AnalyticsRequest& request,
                                              const std::string& sourceTable) {
            const auto weekColumn = eventWeekColumn(request.metric);
            const auto week = weekExpression(weekColumn);
            const auto sector = sectorColumn(request.metric);
            std::ostringstream sql;
            sql << "WITH dimension_rows AS (SELECT " << divisionExpression(sector)
                << " AS \"division\", " << normalizedDimension(sector) << " AS \"sector\", "
                << normalizedPerson(personColumn(request.personRole)) << " AS \"person\" FROM "
                << sourceTable << " WHERE " << numberExpression() << " <> '' AND " << weekColumn
                << " BETWEEN ? AND ? AND " << week << " IS NOT NULL) ";
            return {sql.str(),
                    {std::to_string(domain::toIsoYearWeek(request.period.first)),
                     std::to_string(domain::toIsoYearWeek(request.period.last))}};
        }

        DimensionBase buildStockDimensionBase(const domain::AnalyticsRequest& request,
                                              const std::string& dataset) {
            std::vector<std::string> bindings;
            std::ostringstream sql;
            sql << "WITH " << stockSelectionCte(request, dataset, bindings)
                << ", dimension_rows AS (SELECT points.\"division\" AS \"division\", "
                   "points.\"sector\" AS \"sector\", points.\"person\" AS \"person\" FROM "
                   "selected_snapshots AS snapshots JOIN "
                << quoteTableIdentifier(std::string{kAnalyticsPointTable})
                << " AS points ON points.\"dataset\" = snapshots.\"dataset\" AND "
                   "points.\"metric\" = snapshots.\"metric\" AND "
                   "points.\"observed_iso_week\" = snapshots.\"observed_iso_week\" WHERE "
                   "points.\"person_role\" = ?) ";
            bindings.push_back(personRoleKey(request.personRole));
            return {sql.str(), std::move(bindings)};
        }

        SqlQuery dimensionQuery(const DimensionBase& base, const std::string_view valueColumn,
                                const domain::AnalyticsRequest& request, const bool divisions,
                                const bool sectors) {
            auto bindings = base.bindings;
            std::ostringstream sql;
            sql << base.sql << "SELECT DISTINCT \"" << valueColumn
                << "\" AS \"value\" FROM dimension_rows";
            sql << selectionWhere(request, bindings, divisions, sectors, false);
            sql << " ORDER BY \"value\" COLLATE NOCASE ASC, \"value\" ASC";
            return {sql.str(), std::move(bindings)};
        }

        std::string eventAvailabilitySql(const std::string_view metric,
                                         const std::string& weekColumn,
                                         const std::string_view sourceTable) {
            const auto week = weekExpression(weekColumn);
            const auto valid = week + " IS NOT NULL AND " + numberExpression() + " <> ''";
            return "SELECT '" + std::string{metric} + "' AS \"metric\", MIN(CASE WHEN " + valid +
                   " THEN " + week + " END) AS \"first_iso_week\", MAX(CASE WHEN " + valid +
                   " THEN " + week + " END) AS \"last_iso_week\", CASE WHEN SUM(CASE WHEN " +
                   valid +
                   " THEN 1 ELSE 0 END) > 0 THEN 1 ELSE 0 END AS \"available\", "
                   "CASE WHEN SUM(CASE WHEN " +
                   valid +
                   " THEN 1 ELSE 0 END) > 0 THEN '' ELSE 'event history is unavailable' END AS "
                   "\"reason\" FROM " +
                   std::string{sourceTable};
        }

        std::string eventAvailabilityUnion(const std::string& sourceTable) {
            return eventAvailabilitySql("registered", quoteColumnIdentifier("semana_cadastro"),
                                        sourceTable) +
                   " UNION ALL " +
                   eventAvailabilitySql("executed", quoteColumnIdentifier("semana_executada"),
                                        sourceTable) +
                   " UNION ALL " +
                   eventAvailabilitySql("issued", quoteColumnIdentifier("semana_cadastro"),
                                        sourceTable);
        }

        std::string stockMetricsCte() {
            return "stock_metrics(\"metric\") AS (VALUES ('partial_attention'), ('spg'), "
                   "('apg'), ('apl'), ('pending'), ('pending_deadline'))";
        }

        std::string stockAvailabilitySelect() {
            return "SELECT metrics.\"metric\", MIN(CASE WHEN snapshots.\"complete\" = 1 THEN "
                   "snapshots.\"observed_iso_week\" END) AS \"first_iso_week\", MAX(CASE WHEN "
                   "snapshots.\"complete\" = 1 THEN snapshots.\"observed_iso_week\" END) AS "
                   "\"last_iso_week\", COALESCE(MAX(snapshots.\"complete\"), 0) AS "
                   "\"available\", CASE WHEN COUNT(snapshots.\"metric\") = 0 THEN "
                   "'snapshot history is unavailable' WHEN MAX(snapshots.\"complete\") = 0 "
                   "THEN MAX(snapshots.\"reason\") ELSE '' END AS \"reason\" FROM "
                   "stock_metrics AS metrics LEFT JOIN " +
                   quoteTableIdentifier(std::string{kAnalyticsSnapshotTable}) +
                   " AS snapshots ON snapshots.\"dataset\" = ? AND snapshots.\"metric\" = "
                   "metrics.\"metric\" GROUP BY metrics.\"metric\"";
        }

    } // namespace

    std::string canonicalIsoWeekSqlExpression(const std::string& column) {
        const auto value = "CAST(" + column + " AS INTEGER)";
        const auto storage = "(TYPEOF(" + column + ") = 'integer' OR (TYPEOF(" + column +
                             ") = 'real' AND " + column + " = " + value + ") OR (TYPEOF(" + column +
                             ") = 'text' AND LENGTH(CAST(" + column + " AS BLOB)) = 6 AND " +
                             column + " GLOB '[0-9][0-9][0-9][0-9][0-9][0-9]'))";
        return "CASE WHEN " + storage + " AND " + validIsoWeekExpression(value) + " THEN " + value +
               " ELSE NULL END";
    }

    ActivityAnalyticsSqlBuilder::ActivityAnalyticsSqlBuilder(std::string sourceTable,
                                                             std::string dataset)
        : sourceTable_(std::move(sourceTable)), dataset_(std::move(dataset)) {
        static_cast<void>(quoteTableIdentifier(sourceTable_));
        if (dataset_.empty()) {
            throw std::invalid_argument("analytics dataset is required");
        }
    }

    SqlQuery
    ActivityAnalyticsSqlBuilder::buildSeries(const domain::AnalyticsRequest& request) const {
        if (const auto error = domain::validateAnalyticsRequest(request); error.has_value()) {
            throw std::invalid_argument(*error);
        }
        static_cast<void>(metricKey(request.metric));
        const auto sourceTable = quoteTableIdentifier(sourceTable_);
        return isEventMetric(request.metric) ? buildEventSeries(request, sourceTable)
                                             : buildStockSeries(request, dataset_);
    }

    SqlQuery ActivityAnalyticsSqlBuilder::buildSeries(const domain::AnalyticsRequest& request,
                                                      const SqlWhereClause& sourceFilter) const {
        auto validationRequest = request;
        if (usesPerson(validationRequest.breakdown) && validationRequest.people.empty()) {
            validationRequest.people.emplace_back("source-filtered-report");
        }
        if (const auto error = domain::validateAnalyticsRequest(validationRequest);
            error.has_value()) {
            throw std::invalid_argument(*error);
        }
        if (!isEventMetric(request.metric)) {
            throw std::invalid_argument("source filters are supported only for event analytics");
        }
        static_cast<void>(metricKey(request.metric));
        return buildEventSeries(request, quoteTableIdentifier(sourceTable_), sourceFilter, true);
    }

    ActivityAnalyticsDimensionQueries ActivityAnalyticsSqlBuilder::buildDimensionValues(
        const domain::AnalyticsRequest& request) const {
        if (!domain::isValidPeriod(request.period)) {
            throw std::invalid_argument("analytics period is invalid");
        }
        static_cast<void>(metricKey(request.metric));
        const auto sourceTable = quoteTableIdentifier(sourceTable_);
        const auto metric = isEventMetric(request.metric)
                                ? buildEventDimensionBase(request, sourceTable)
                                : buildStockDimensionBase(request, dataset_);
        return {
            dimensionQuery(metric, "division", request, false, false),
            dimensionQuery(metric, "sector", request, true, false),
            dimensionQuery(metric, "person", request, true, true),
        };
    }

    SqlQuery ActivityAnalyticsSqlBuilder::buildEventAvailability() const {
        const auto sourceTable = quoteTableIdentifier(sourceTable_);
        return {eventAvailabilityUnion(sourceTable) + " ORDER BY \"metric\"", {}};
    }

    SqlQuery ActivityAnalyticsSqlBuilder::buildStockAvailability() const {
        return {"WITH " + stockMetricsCte() + ' ' + stockAvailabilitySelect() +
                    " ORDER BY metrics.\"metric\"",
                {dataset_}};
    }

    SqlQuery ActivityAnalyticsSqlBuilder::buildAvailability() const {
        const auto sourceTable = quoteTableIdentifier(sourceTable_);
        return {"WITH " + stockMetricsCte() + ' ' + eventAvailabilityUnion(sourceTable) +
                    " UNION ALL " + stockAvailabilitySelect() + " ORDER BY \"metric\"",
                {dataset_}};
    }

    SqlQuery ActivityAnalyticsSqlBuilder::buildSnapshotMetadata(
        const domain::AnalyticsRequest& request) const {
        if (!domain::isValidPeriod(request.period)) {
            throw std::invalid_argument("analytics period is invalid");
        }
        if (isEventMetric(request.metric)) {
            throw std::invalid_argument("event analytics has no snapshot metadata");
        }

        std::vector<std::string> bindings;
        const bool requireComplete = request.metric != domain::AnalyticsMetric::PartialAttention;
        std::ostringstream sql;
        sql << "WITH " << stockSelectionCte(request, dataset_, bindings, requireComplete)
            << " SELECT \"bucket_key\", \"observed_iso_week\", \"source_revision\", "
               "\"complete\", \"reason\" FROM selected_snapshots ORDER BY \"bucket_key\", "
               "\"observed_iso_week\"";
        return {sql.str(), std::move(bindings)};
    }

} // namespace ssa::query
