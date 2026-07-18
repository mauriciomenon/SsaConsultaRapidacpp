#include "infra/sqlite/SqliteSsaAnalyticsPort.h"

#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        constexpr std::string_view kDataset = "SSA";
        constexpr long long kProjectionSchemaVersion = 1;

        constexpr std::array kAllMetrics{
            domain::AnalyticsMetric::Registered,
            domain::AnalyticsMetric::Executed,
            domain::AnalyticsMetric::PartialAttention,
            domain::AnalyticsMetric::Spg,
            domain::AnalyticsMetric::Apg,
            domain::AnalyticsMetric::Apl,
            domain::AnalyticsMetric::Pending,
            domain::AnalyticsMetric::Issued,
            domain::AnalyticsMetric::PendingDeadline,
        };

        constexpr std::array kStockMetrics{
            domain::AnalyticsMetric::PartialAttention,
            domain::AnalyticsMetric::Spg,
            domain::AnalyticsMetric::Apg,
            domain::AnalyticsMetric::Apl,
            domain::AnalyticsMetric::Pending,
            domain::AnalyticsMetric::PendingDeadline,
        };

        struct ProjectionState final {
            bool available{false};
            std::string reason;
        };

        struct SnapshotIdentity final {
            int observedIsoYearWeek{0};
            std::string sourceRevision;
        };

        struct SnapshotSelection final {
            std::map<std::string, SnapshotIdentity> buckets;
            std::optional<int> latestObservedIsoYearWeek;
            std::string latestSourceRevision;
            bool complete{true};
            std::string reason;
        };

        [[noreturn]] void throwInvalidAnalyticsData(std::string diagnostic) {
            throw ports::OperationError("Falha ao ler analises", std::move(diagnostic));
        }

        [[noreturn]] void throwAnalyticsSettingsWriteError(std::string diagnostic) {
            throw ports::OperationError("Falha ao salvar configuracao de analises",
                                        std::move(diagnostic));
        }

        bool isEventMetric(const domain::AnalyticsMetric metric) {
            switch (metric) {
            case domain::AnalyticsMetric::Registered:
            case domain::AnalyticsMetric::Executed:
            case domain::AnalyticsMetric::Issued:
                return true;
            case domain::AnalyticsMetric::PartialAttention:
            case domain::AnalyticsMetric::Spg:
            case domain::AnalyticsMetric::Apg:
            case domain::AnalyticsMetric::Apl:
            case domain::AnalyticsMetric::Pending:
            case domain::AnalyticsMetric::PendingDeadline:
                return false;
            }
            throw std::invalid_argument("unknown analytics metric");
        }

        domain::AnalyticsMetric metricFromKey(const std::string_view key) {
            if (key == "registered") {
                return domain::AnalyticsMetric::Registered;
            }
            if (key == "executed") {
                return domain::AnalyticsMetric::Executed;
            }
            if (key == "partial_attention") {
                return domain::AnalyticsMetric::PartialAttention;
            }
            if (key == "spg") {
                return domain::AnalyticsMetric::Spg;
            }
            if (key == "apg") {
                return domain::AnalyticsMetric::Apg;
            }
            if (key == "apl") {
                return domain::AnalyticsMetric::Apl;
            }
            if (key == "pending") {
                return domain::AnalyticsMetric::Pending;
            }
            if (key == "issued") {
                return domain::AnalyticsMetric::Issued;
            }
            if (key == "pending_deadline") {
                return domain::AnalyticsMetric::PendingDeadline;
            }
            throwInvalidAnalyticsData("unknown analytics metric key: " + std::string{key});
        }

        domain::RegistrationCohort cohortFromKey(const std::string_view key) {
            if (key == "in_period") {
                return domain::RegistrationCohort::RegisteredInPeriod;
            }
            if (key == "before") {
                return domain::RegistrationCohort::RegisteredBeforePeriod;
            }
            if (key == "unknown") {
                return domain::RegistrationCohort::RegistrationUnknown;
            }
            throwInvalidAnalyticsData("unknown analytics registration cohort: " + std::string{key});
        }

        domain::DeadlineClass deadlineClassFromKey(const std::string_view key) {
            if (key == "on_time") {
                return domain::DeadlineClass::OnTime;
            }
            if (key == "warning") {
                return domain::DeadlineClass::Warning;
            }
            if (key == "overdue") {
                return domain::DeadlineClass::Overdue;
            }
            if (key == "not_applicable_or_unknown") {
                return domain::DeadlineClass::NotApplicableOrUnknown;
            }
            throwInvalidAnalyticsData("unknown analytics deadline class: " + std::string{key});
        }

        void requireColumnCount(const SqliteStatement& statement, const int expected,
                                const std::string_view context) {
            if (statement.columnCount() != expected) {
                throwInvalidAnalyticsData(std::string{context} + " returned " +
                                          std::to_string(statement.columnCount()) +
                                          " columns instead of " + std::to_string(expected));
            }
        }

        void bindQuery(SqliteStatement& statement, const query::SqlQuery& query) {
            const int expected = sqlite3_bind_parameter_count(statement.handle());
            if (expected < 0 || static_cast<std::size_t>(expected) != query.bindings.size()) {
                throwInvalidAnalyticsData("analytics query binding count mismatch");
            }
            for (std::size_t index = 0; index < query.bindings.size(); ++index) {
                statement.bindTextOneBased(static_cast<int>(index + 1), query.bindings[index]);
            }
        }

        std::optional<int> readOptionalIsoYearWeek(const SqliteStatement& statement,
                                                   const int column,
                                                   const std::string_view context) {
            const int type = sqlite3_column_type(statement.handle(), column);
            if (type == SQLITE_NULL) {
                return std::nullopt;
            }
            if (type != SQLITE_INTEGER) {
                throwInvalidAnalyticsData(std::string{context} + " is not an integer");
            }
            const long long value = statement.columnInt64(column);
            if (value < std::numeric_limits<int>::min() ||
                value > std::numeric_limits<int>::max()) {
                throwInvalidAnalyticsData(std::string{context} + " is outside the integer range");
            }
            const int isoYearWeek = static_cast<int>(value);
            if (!domain::isValidIsoWeek({isoYearWeek / 100, isoYearWeek % 100})) {
                throwInvalidAnalyticsData(std::string{context} + " is not a valid ISO week");
            }
            return isoYearWeek;
        }

        bool readBoolean(const SqliteStatement& statement, const int column,
                         const std::string_view context) {
            if (sqlite3_column_type(statement.handle(), column) != SQLITE_INTEGER) {
                throwInvalidAnalyticsData(std::string{context} + " is not an integer boolean");
            }
            const long long value = statement.columnInt64(column);
            if (value != 0 && value != 1) {
                throwInvalidAnalyticsData(std::string{context} + " is not zero or one");
            }
            return value == 1;
        }

        std::int64_t readCount(const SqliteStatement& statement, const int column) {
            if (sqlite3_column_type(statement.handle(), column) != SQLITE_INTEGER) {
                throwInvalidAnalyticsData("analytics count is not an integer");
            }
            const auto value = static_cast<std::int64_t>(statement.columnInt64(column));
            if (value < 0) {
                throwInvalidAnalyticsData("analytics count is negative");
            }
            return value;
        }

        void checkedAdd(std::int64_t& total, const std::int64_t value) {
            if (value < 0 || total > std::numeric_limits<std::int64_t>::max() - value) {
                throwInvalidAnalyticsData("analytics count overflow");
            }
            total += value;
        }

        ProjectionState projectionState(sqlite3* db,
                                        const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement tables(
                db, "SELECT COUNT(*) FROM sqlite_schema WHERE type='table' AND name IN (?, ?, ?)",
                busyCancellationObserved);
            tables.bindTextOneBased(1, "activity_analytics_meta");
            tables.bindTextOneBased(2, "activity_analytics_snapshot");
            tables.bindTextOneBased(3, "activity_analytics_point");
            if (!tables.step()) {
                throwInvalidAnalyticsData("analytics schema inspection returned no row");
            }
            if (tables.columnInt64(0) != 3) {
                return {false, "analytics projection schema is unavailable"};
            }

            SqliteStatement metadata(
                db, "SELECT schema_version FROM activity_analytics_meta WHERE dataset=?",
                busyCancellationObserved);
            metadata.bindTextOneBased(1, std::string{kDataset});
            if (!metadata.step()) {
                return {false, "analytics projection metadata is unavailable"};
            }
            if (sqlite3_column_type(metadata.handle(), 0) != SQLITE_INTEGER) {
                throwInvalidAnalyticsData("analytics schema version is not an integer");
            }
            if (metadata.columnInt64(0) != kProjectionSchemaVersion) {
                return {false, "analytics projection schema version is incompatible"};
            }
            return {true, {}};
        }

        std::optional<int>
        storedWarningWindowDays(sqlite3* db, const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement setting(
                db, "SELECT warning_window_days FROM activity_analytics_meta WHERE dataset=?",
                busyCancellationObserved);
            setting.bindTextOneBased(1, std::string{kDataset});
            if (!setting.step() || sqlite3_column_type(setting.handle(), 0) == SQLITE_NULL) {
                return std::nullopt;
            }
            if (sqlite3_column_type(setting.handle(), 0) != SQLITE_INTEGER) {
                throwInvalidAnalyticsData("analytics warning window is not an integer");
            }
            const long long value = setting.columnInt64(0);
            if (value < 0 || value > 365) {
                throwInvalidAnalyticsData("analytics warning window is outside 0..365");
            }
            return static_cast<int>(value);
        }

        SnapshotSelection readSnapshotSelection(sqlite3* db, const query::SqlQuery& query,
                                                const std::stop_token& stopToken,
                                                const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(db, query.sql, busyCancellationObserved);
            bindQuery(statement, query);

            SnapshotSelection selection;
            while (statement.step()) {
                throwIfCanceled(stopToken);
                requireColumnCount(statement, 5, "analytics snapshot metadata query");
                const std::string bucket = statement.columnText(0);
                const auto observed =
                    readOptionalIsoYearWeek(statement, 1, "analytics snapshot observed ISO week");
                if (!observed.has_value()) {
                    throwInvalidAnalyticsData("analytics snapshot has no observed ISO week");
                }
                const std::string revision = statement.columnText(2);
                if (revision.empty()) {
                    throwInvalidAnalyticsData("analytics snapshot has no source revision");
                }
                const bool complete = readBoolean(statement, 3, "analytics snapshot completeness");
                const std::string reason = statement.columnText(4);
                if (!selection.buckets.emplace(bucket, SnapshotIdentity{*observed, revision})
                         .second) {
                    throwInvalidAnalyticsData("analytics snapshot metadata has a duplicate bucket");
                }
                if (!selection.latestObservedIsoYearWeek.has_value() ||
                    *observed > *selection.latestObservedIsoYearWeek) {
                    selection.latestObservedIsoYearWeek = observed;
                    selection.latestSourceRevision = revision;
                }
                if (!complete) {
                    selection.complete = false;
                    if (selection.reason.empty()) {
                        selection.reason =
                            reason.empty() ? "analytics snapshot is incomplete" : reason;
                    }
                }
            }
            throwIfCanceled(stopToken);
            return selection;
        }

        void readSeriesRows(sqlite3* db, const query::SqlQuery& query,
                            const domain::AnalyticsMetric metric, const bool eventMetric,
                            const SnapshotSelection* snapshots, const std::stop_token& stopToken,
                            const std::atomic_bool* busyCancellationObserved,
                            domain::AnalyticsSeriesResult& result) {
            SqliteStatement statement(db, query.sql, busyCancellationObserved);
            bindQuery(statement, query);

            while (statement.step()) {
                throwIfCanceled(stopToken);
                requireColumnCount(statement, 9, "analytics series query");
                domain::AnalyticsPoint point{
                    .bucketKey = statement.columnText(0),
                    .division = statement.columnText(1),
                    .sector = statement.columnText(2),
                    .person = statement.columnText(3),
                    .cohort = cohortFromKey(statement.columnText(4)),
                    .deadlineClass = deadlineClassFromKey(statement.columnText(5)),
                    .count = readCount(statement, 8),
                };
                const auto observed =
                    readOptionalIsoYearWeek(statement, 6, "analytics series observed ISO week");
                const std::string sourceRevision = statement.columnText(7);
                if (eventMetric) {
                    if (observed.has_value() || !sourceRevision.empty()) {
                        throwInvalidAnalyticsData(
                            "event analytics unexpectedly references a snapshot");
                    }
                } else {
                    if (snapshots == nullptr || !observed.has_value()) {
                        throwInvalidAnalyticsData("stock analytics lacks snapshot metadata");
                    }
                    const auto identity = snapshots->buckets.find(point.bucketKey);
                    if (identity == snapshots->buckets.end() ||
                        identity->second.observedIsoYearWeek != *observed ||
                        identity->second.sourceRevision != sourceRevision) {
                        throwInvalidAnalyticsData(
                            "stock analytics does not match its snapshot metadata");
                    }
                }
                if (metric == domain::AnalyticsMetric::PendingDeadline &&
                    point.deadlineClass == domain::DeadlineClass::NotApplicableOrUnknown) {
                    checkedAdd(result.excludedForDataQuality, point.count);
                }
                result.points.push_back(std::move(point));
            }
            throwIfCanceled(stopToken);
        }

        std::vector<std::string>
        readDimensionValues(sqlite3* db, const query::SqlQuery& query,
                            const std::stop_token& stopToken,
                            const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(db, query.sql, busyCancellationObserved);
            bindQuery(statement, query);
            std::vector<std::string> values;
            while (statement.step()) {
                throwIfCanceled(stopToken);
                requireColumnCount(statement, 1, "analytics dimension query");
                values.push_back(statement.columnText(0));
            }
            throwIfCanceled(stopToken);
            return values;
        }

        std::vector<domain::AnalyticsMetricAvailability> emptyAvailability() {
            std::vector<domain::AnalyticsMetricAvailability> result;
            result.reserve(kAllMetrics.size());
            for (const auto metric : kAllMetrics) {
                result.push_back({
                    .metric = metric,
                    .available = false,
                    .reason = "analytics availability was not reported",
                });
            }
            return result;
        }

        void readAvailabilityRows(sqlite3* db, const query::SqlQuery& query,
                                  const std::stop_token& stopToken,
                                  const std::atomic_bool* busyCancellationObserved,
                                  std::vector<domain::AnalyticsMetricAvailability>& result) {
            SqliteStatement statement(db, query.sql, busyCancellationObserved);
            bindQuery(statement, query);
            while (statement.step()) {
                throwIfCanceled(stopToken);
                requireColumnCount(statement, 5, "analytics availability query");
                const auto metric = metricFromKey(statement.columnText(0));
                const auto found =
                    std::ranges::find(result, metric, &domain::AnalyticsMetricAvailability::metric);
                if (found == result.end()) {
                    throwInvalidAnalyticsData("analytics availability returned an unknown metric");
                }
                found->firstIsoYearWeek =
                    readOptionalIsoYearWeek(statement, 1, "analytics first available ISO week");
                found->lastIsoYearWeek =
                    readOptionalIsoYearWeek(statement, 2, "analytics last available ISO week");
                found->available = readBoolean(statement, 3, "analytics availability flag");
                found->reason = statement.columnText(4);
                if (found->available &&
                    (!found->firstIsoYearWeek.has_value() || !found->lastIsoYearWeek.has_value())) {
                    throwInvalidAnalyticsData(
                        "available analytics metric has no observed ISO week range");
                }
                if (found->firstIsoYearWeek.has_value() && found->lastIsoYearWeek.has_value() &&
                    *found->firstIsoYearWeek > *found->lastIsoYearWeek) {
                    throwInvalidAnalyticsData("analytics availability range is reversed");
                }
            }
            throwIfCanceled(stopToken);
        }

        void markStocksUnavailable(std::vector<domain::AnalyticsMetricAvailability>& result,
                                   const std::string& reason) {
            for (const auto metric : kStockMetrics) {
                const auto found =
                    std::ranges::find(result, metric, &domain::AnalyticsMetricAvailability::metric);
                if (found != result.end()) {
                    found->available = false;
                    found->reason = reason;
                }
            }
        }

    } // namespace

    SqliteSsaAnalyticsPort::SqliteSsaAnalyticsPort(std::filesystem::path dbPath)
        : dbPath_(std::move(dbPath)) {}

    domain::AnalyticsSeriesResult
    SqliteSsaAnalyticsPort::series(const domain::AnalyticsRequest& request,
                                   const std::stop_token stopToken) const {
        const bool eventMetric = isEventMetric(request.metric);
        const auto seriesQuery = queryBuilder_.buildSeries(request);
        const auto metadataQuery = eventMetric ? std::optional<query::SqlQuery>{}
                                               : queryBuilder_.buildSnapshotMetadata(request);
        throwIfCanceled(stopToken);

        SqliteConnection connection(dbPath_);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteReadTransaction transaction(connection.handle(), stopToken,
                                          busy.cancellationObserved());

        domain::AnalyticsSeriesResult result;
        SnapshotSelection snapshots;
        if (!eventMetric) {
            const auto state = projectionState(connection.handle(), busy.cancellationObserved());
            if (!state.available) {
                result.complete = false;
                result.unavailableReason = state.reason;
                transaction.commit();
                return result;
            }
            snapshots = readSnapshotSelection(connection.handle(), *metadataQuery, stopToken,
                                              busy.cancellationObserved());
            result.sourceRevision = snapshots.latestSourceRevision;
            result.observedIsoYearWeek = snapshots.latestObservedIsoYearWeek;
            if (!snapshots.complete) {
                result.complete = false;
                result.unavailableReason = snapshots.reason;
                transaction.commit();
                return result;
            }
            if (snapshots.buckets.empty()) {
                if (request.grain == domain::TimeGrain::WholePeriod) {
                    result.complete = false;
                    result.unavailableReason = "snapshot history is unavailable";
                }
                transaction.commit();
                return result;
            }
            result.observations.reserve(snapshots.buckets.size());
            for (const auto& [bucketKey, identity] : snapshots.buckets) {
                result.observations.push_back({
                    .bucketKey = bucketKey,
                    .observedIsoYearWeek = identity.observedIsoYearWeek,
                    .sourceRevision = identity.sourceRevision,
                });
            }
        }

        readSeriesRows(connection.handle(), seriesQuery, request.metric, eventMetric,
                       eventMetric ? nullptr : &snapshots, stopToken, busy.cancellationObserved(),
                       result);
        transaction.commit();
        return result;
    }

    domain::AnalyticsDimensionValues
    SqliteSsaAnalyticsPort::dimensionValues(const domain::AnalyticsRequest& request,
                                            const std::stop_token stopToken) const {
        const bool eventMetric = isEventMetric(request.metric);
        const auto queries = queryBuilder_.buildDimensionValues(request);
        throwIfCanceled(stopToken);

        SqliteConnection connection(dbPath_);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteReadTransaction transaction(connection.handle(), stopToken,
                                          busy.cancellationObserved());
        if (!eventMetric &&
            !projectionState(connection.handle(), busy.cancellationObserved()).available) {
            transaction.commit();
            return {};
        }

        domain::AnalyticsDimensionValues result{
            .divisions = readDimensionValues(connection.handle(), queries.divisions, stopToken,
                                             busy.cancellationObserved()),
            .sectors = readDimensionValues(connection.handle(), queries.sectors, stopToken,
                                           busy.cancellationObserved()),
            .people = readDimensionValues(connection.handle(), queries.people, stopToken,
                                          busy.cancellationObserved()),
        };
        transaction.commit();
        return result;
    }

    std::vector<domain::AnalyticsMetricAvailability>
    SqliteSsaAnalyticsPort::availability(const std::stop_token stopToken) const {
        const auto eventQuery = queryBuilder_.buildEventAvailability();
        const auto stockQuery = queryBuilder_.buildStockAvailability();
        throwIfCanceled(stopToken);

        SqliteConnection connection(dbPath_);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteReadTransaction transaction(connection.handle(), stopToken,
                                          busy.cancellationObserved());

        auto result = emptyAvailability();
        readAvailabilityRows(connection.handle(), eventQuery, stopToken,
                             busy.cancellationObserved(), result);
        const auto state = projectionState(connection.handle(), busy.cancellationObserved());
        if (state.available) {
            readAvailabilityRows(connection.handle(), stockQuery, stopToken,
                                 busy.cancellationObserved(), result);
        } else {
            markStocksUnavailable(result, state.reason);
        }
        transaction.commit();
        return result;
    }

    std::optional<int>
    SqliteSsaAnalyticsPort::warningWindowDays(const std::stop_token stopToken) const {
        throwIfCanceled(stopToken);
        SqliteConnection connection(dbPath_);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteReadTransaction transaction(connection.handle(), stopToken,
                                          busy.cancellationObserved());
        const auto state = projectionState(connection.handle(), busy.cancellationObserved());
        if (!state.available) {
            transaction.commit();
            return std::nullopt;
        }
        const auto value =
            storedWarningWindowDays(connection.handle(), busy.cancellationObserved());
        throwIfCanceled(stopToken);
        transaction.commit();
        return value;
    }

    void SqliteSsaAnalyticsPort::setWarningWindowDays(const int days,
                                                      const std::stop_token stopToken) {
        if (days < 0 || days > 365) {
            throw std::invalid_argument("warning window must be between 0 and 365 days");
        }
        throwIfCanceled(stopToken);
        const SqliteDatabaseWriteLock writeLock(dbPath_);
        if (!writeLock.acquired()) {
            throwIfCanceled(stopToken);
            throwAnalyticsSettingsWriteError(std::string{writeLock.diagnostic()});
        }

        SqliteConnection connection(dbPath_, SqliteOpenMode::ReadWrite);
        SqliteBusyHandler busy(connection.handle(), stopToken);
        SqliteProgressHandler progress(connection.handle(), stopToken);
        SqliteWriteTransaction transaction(connection.handle(), busy.cancellationObserved());
        const auto state = projectionState(connection.handle(), busy.cancellationObserved());
        if (!state.available) {
            throwAnalyticsSettingsWriteError(state.reason);
        }

        SqliteStatement update(
            connection.handle(),
            "UPDATE activity_analytics_meta SET warning_window_days=? WHERE dataset=?",
            busy.cancellationObserved());
        update.bindInt64OneBased(1, days);
        update.bindTextOneBased(2, std::string{kDataset});
        update.executeAndReset();
        if (sqlite3_changes(connection.handle()) != 1) {
            throwAnalyticsSettingsWriteError("analytics settings metadata was not updated");
        }
        throwIfCanceled(stopToken);
        transaction.commit();
    }

} // namespace ssa::infra::sqlite
