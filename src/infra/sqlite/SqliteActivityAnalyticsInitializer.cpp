#include "infra/sqlite/SqliteActivityAnalyticsInitializer.h"

#include "domain/ActivityAnalyticsTypes.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "query/ActivityAnalyticsSqlBuilder.h"

#include <string>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        constexpr std::string_view kDataset = "SSA";
        constexpr int kSchemaVersion = 1;
        constexpr int kMetricCount = 6;

        enum class BaselineState { Fresh, Ready, RepairCurrent };

        BaselineState classifyBaseline(sqlite3* db, const int observedIsoYearWeek,
                                       const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement tables(
                db,
                "SELECT SUM(name='activity_analytics_meta'), "
                "SUM(name='activity_analytics_snapshot'), "
                "SUM(name='activity_analytics_point') FROM sqlite_master WHERE type='table' "
                "AND name IN ('activity_analytics_meta', 'activity_analytics_snapshot', "
                "'activity_analytics_point')",
                busyCancellationObserved);
            if (!tables.step()) {
                throw ports::OperationError("Falha ao inicializar analises",
                                            "analytics table inventory returned no row");
            }
            const bool hasMeta = tables.columnInt64(0) == 1;
            const bool hasSnapshot = tables.columnInt64(1) == 1;
            const bool hasPoint = tables.columnInt64(2) == 1;
            const int tableCount = static_cast<int>(hasMeta) + static_cast<int>(hasSnapshot) +
                                   static_cast<int>(hasPoint);
            if (tableCount == 0) {
                return BaselineState::Fresh;
            }
            if (!hasMeta || !hasSnapshot || (tableCount != 2 && tableCount != 3)) {
                throw ports::OperationError(
                    "Falha ao inicializar analises",
                    "activity analytics schema is partial and cannot be repaired safely");
            }

            SqliteStatement weeks(
                db,
                "SELECT observed_iso_week, COUNT(*), COUNT(DISTINCT metric), "
                "COALESCE(SUM(CASE WHEN metric IN ('partial_attention','spg','apg','apl',"
                "'pending','pending_deadline') THEN 1 ELSE 0 END),0), "
                "COUNT(DISTINCT source_revision), COALESCE(MIN(LENGTH(source_revision)),0), "
                "COUNT(DISTINCT source_fingerprint), "
                "COALESCE(MIN(LENGTH(source_fingerprint)),0), COUNT(DISTINCT observed_date), "
                "COALESCE(SUM(CASE WHEN LENGTH(observed_date)=10 AND "
                "DATE(observed_date)=observed_date THEN 1 ELSE 0 END),0), "
                "COALESCE(SUM(CASE WHEN (metric='partial_attention' AND complete=0 AND "
                "reason<>'') OR (metric IN ('spg','apg','apl','pending','pending_deadline') "
                "AND complete=1 AND reason='') THEN 1 ELSE 0 END),0) "
                "FROM activity_analytics_snapshot WHERE dataset=? GROUP BY observed_iso_week "
                "ORDER BY observed_iso_week",
                busyCancellationObserved);
            weeks.bindTextOneBased(1, std::string{kDataset});
            int weekCount = 0;
            int firstWeek = 0;
            int lastWeek = 0;
            bool currentInvalid = false;
            while (weeks.step()) {
                const int week = static_cast<int>(weeks.columnInt64(0));
                if (sqlite3_column_type(weeks.handle(), 0) != SQLITE_INTEGER) {
                    throw ports::OperationError(
                        "Falha ao inicializar analises",
                        "activity analytics snapshot ISO week is not stored as INTEGER");
                }
                if (weekCount == 0) {
                    firstWeek = week;
                }
                lastWeek = week;
                ++weekCount;
                const domain::IsoWeek isoWeek{week / 100, week % 100};
                const bool valid =
                    domain::isValidIsoWeek(isoWeek) && weeks.columnInt64(1) == kMetricCount &&
                    weeks.columnInt64(2) == kMetricCount && weeks.columnInt64(3) == kMetricCount &&
                    weeks.columnInt64(4) == 1 && weeks.columnInt64(5) > 0 &&
                    weeks.columnInt64(6) == 1 && weeks.columnInt64(7) > 0 &&
                    weeks.columnInt64(8) == 1 && weeks.columnInt64(9) == kMetricCount &&
                    weeks.columnInt64(10) == kMetricCount;
                if (valid) {
                    continue;
                }
                if (week != observedIsoYearWeek) {
                    throw ports::OperationError(
                        "Falha ao inicializar analises",
                        "activity analytics historical snapshot is invalid for ISO week " +
                            std::to_string(week));
                }
                currentInvalid = true;
            }
            if (weekCount == 0) {
                throw ports::OperationError("Falha ao inicializar analises",
                                            "activity analytics snapshot history is empty");
            }

            SqliteStatement meta(
                db,
                "SELECT schema_version, active_source_revision, baseline_iso_week FROM "
                "activity_analytics_meta WHERE dataset=?",
                busyCancellationObserved);
            meta.bindTextOneBased(1, std::string{kDataset});
            bool validMeta = false;
            if (meta.step()) {
                const int baselineWeek = static_cast<int>(meta.columnInt64(2));
                const domain::IsoWeek baseline{baselineWeek / 100, baselineWeek % 100};
                validMeta = sqlite3_column_type(meta.handle(), 0) == SQLITE_INTEGER &&
                            sqlite3_column_type(meta.handle(), 1) == SQLITE_TEXT &&
                            sqlite3_column_type(meta.handle(), 2) == SQLITE_INTEGER &&
                            meta.columnInt64(0) == kSchemaVersion && !meta.columnText(1).empty() &&
                            baselineWeek == firstWeek && domain::isValidIsoWeek(baseline) &&
                            !meta.step();
            }
            if (!hasPoint) {
                if (weekCount != 1 || firstWeek != observedIsoYearWeek) {
                    throw ports::OperationError(
                        "Falha ao inicializar analises",
                        "activity analytics point table is missing for historical snapshots");
                }
                return BaselineState::RepairCurrent;
            }
            SqliteStatement pointSchema(
                db,
                "SELECT dataset, observed_iso_week, metric, division, sector, person_role, "
                "person, registration_iso_week, deadline_source_state, deadline_offset_days, "
                "count FROM activity_analytics_point LIMIT 0",
                busyCancellationObserved);
            static_cast<void>(pointSchema.step());
            const auto registrationWeek =
                query::canonicalIsoWeekSqlExpression("points.registration_iso_week");
            SqliteStatement invalidPoint(
                db,
                "SELECT 1 FROM activity_analytics_point AS points LEFT JOIN "
                "activity_analytics_snapshot AS snapshots ON snapshots.dataset=points.dataset "
                "AND snapshots.observed_iso_week=points.observed_iso_week AND "
                "snapshots.metric=points.metric WHERE points.dataset=? AND "
                "(?=0 OR points.observed_iso_week<>?) AND ("
                "snapshots.metric IS NULL OR snapshots.complete<>1 OR points.metric NOT IN "
                "('spg','apg','apl','pending','pending_deadline') OR points.person_role NOT IN "
                "('requester','planner','executor') OR TYPEOF(points.observed_iso_week)<>"
                "'integer' OR TYPEOF(points.division)<>'text' OR points.division='' OR "
                "TYPEOF(points.sector)<>'text' OR points.sector='' OR TYPEOF(points.person)<>"
                "'text' OR points.person='' OR TYPEOF(points.deadline_source_state)<>'text' OR "
                "TYPEOF(points.count)<>'integer' OR points.count<=0 OR "
                "(points.registration_iso_week IS NOT NULL AND " +
                    registrationWeek +
                    " IS NULL) OR (points.deadline_offset_days IS NOT NULL AND "
                    "TYPEOF(points.deadline_offset_days)<>'integer') OR "
                    "(points.metric<>'pending_deadline' AND "
                    "(points.deadline_source_state<>'' OR "
                    "points.deadline_offset_days IS NOT NULL))) LIMIT 1",
                busyCancellationObserved);
            invalidPoint.bindTextOneBased(1, std::string{kDataset});
            invalidPoint.bindInt64OneBased(2, currentInvalid ? 1 : 0);
            invalidPoint.bindInt64OneBased(3, observedIsoYearWeek);
            if (invalidPoint.step()) {
                throw ports::OperationError(
                    "Falha ao inicializar analises",
                    "activity analytics point content violates the projection contract");
            }
            SqliteStatement duplicatePoint(
                db,
                "SELECT 1 FROM activity_analytics_point WHERE dataset=? AND "
                "(?=0 OR observed_iso_week<>?) GROUP BY dataset, "
                "observed_iso_week, metric, division, sector, person_role, person, "
                "registration_iso_week, deadline_source_state, deadline_offset_days "
                "HAVING COUNT(*)>1 LIMIT 1",
                busyCancellationObserved);
            duplicatePoint.bindTextOneBased(1, std::string{kDataset});
            duplicatePoint.bindInt64OneBased(2, currentInvalid ? 1 : 0);
            duplicatePoint.bindInt64OneBased(3, observedIsoYearWeek);
            if (duplicatePoint.step()) {
                throw ports::OperationError("Falha ao inicializar analises",
                                            "activity analytics point groups contain duplicates");
            }
            if (!validMeta) {
                if (weekCount != 1 || firstWeek != observedIsoYearWeek) {
                    throw ports::OperationError(
                        "Falha ao inicializar analises",
                        "activity analytics metadata is invalid for historical snapshots");
                }
                return BaselineState::RepairCurrent;
            }
            if (currentInvalid) {
                if (observedIsoYearWeek != lastWeek) {
                    throw ports::OperationError(
                        "Falha ao inicializar analises",
                        "activity analytics repair target is not the latest snapshot");
                }
                return BaselineState::RepairCurrent;
            }
            return BaselineState::Ready;
        }

    } // namespace

    ActivityAnalyticsCaptureResult SqliteActivityAnalyticsInitializer::initialize(
        const std::filesystem::path& dbPath, const int observedIsoYearWeek,
        std::string observedDate, const std::stop_token& stopToken) {
        throwIfCanceled(stopToken);
        const SqliteDatabaseWriteLock writeLock(dbPath);
        if (!writeLock.acquired()) {
            throwIfCanceled(stopToken);
            throw ports::OperationError("Falha ao inicializar analises",
                                        std::string{writeLock.diagnostic()});
        }

        SqliteConnection connection(dbPath, SqliteOpenMode::ReadWrite);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());
        throwIfCanceled(stopToken);
        const auto baselineState =
            classifyBaseline(connection.handle(), observedIsoYearWeek, busy.cancellationObserved());
        if (baselineState == BaselineState::Ready) {
            transaction.commit();
            return {};
        }

        throwIfCanceled(stopToken);
        const auto fingerprint = SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
            connection.handle(), "ssa_table", stopToken, busy.cancellationObserved());
        ActivityAnalyticsCaptureContext context{
            .observedIsoYearWeek = observedIsoYearWeek,
            .observedDate = std::move(observedDate),
            .sourceRevision = "baseline-" + fingerprint,
            .sourceFingerprint = fingerprint,
        };
        const auto result = SqliteActivityAnalyticsProjection::capture(
            connection.handle(), "ssa_table", context, stopToken, busy.cancellationObserved());
        throwIfCanceled(stopToken);
        transaction.commit();
        return result;
    }

} // namespace ssa::infra::sqlite
