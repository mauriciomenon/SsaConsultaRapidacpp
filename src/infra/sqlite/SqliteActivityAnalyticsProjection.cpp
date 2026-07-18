#include "infra/sqlite/SqliteActivityAnalyticsProjection.h"

#include "domain/ActivityAnalyticsTypes.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "query/ActivityAnalyticsSqlBuilder.h"
#include "query/SqlQueryText.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        constexpr std::string_view kDataset = "SSA";
        constexpr int kSchemaVersion = 1;

        struct StockMetric final {
            std::string_view key;
            std::string_view predicate;
            bool complete{false};
            std::string_view reason;
            bool deadline{false};
        };

        struct PersonDimension final {
            std::string_view role;
            std::string_view column;
        };

        constexpr std::array kMetrics{
            StockMetric{"partial_attention", "0", false,
                        "complete partial-attention source is unavailable", false},
            StockMetric{"spg", "UPPER(TRIM(COALESCE(situacao, ''))) = 'SPG'", true, "", false},
            StockMetric{"apg", "UPPER(TRIM(COALESCE(situacao, ''))) = 'APG'", true, "", false},
            StockMetric{"apl", "UPPER(TRIM(COALESCE(situacao, ''))) = 'APL'", true, "", false},
            StockMetric{"pending",
                        "UPPER(TRIM(COALESCE(situacao, ''))) NOT IN ('SCA', 'SES', 'STE')", true,
                        "", false},
            StockMetric{"pending_deadline",
                        "UPPER(TRIM(COALESCE(situacao, ''))) NOT IN ('SCA', 'SES', 'STE')", true,
                        "", true},
        };

        constexpr std::array kPeople{
            PersonDimension{"requester", "solicitante"},
            PersonDimension{"planner", "responsavel_programacao"},
            PersonDimension{"executor", "responsavel_execucao"},
        };

        constexpr std::array<std::string_view, 11> kCanonicalSourceColumns{
            "numero_ssa",     "semana_cadastro",         "semana_executada",
            "setor_executor", "setor_emissor",           "situacao",
            "solicitante",    "responsavel_programacao", "responsavel_execucao",
            "prazo_limite",   "status_execucao_prazo",
        };

        std::string formatFingerprint(const std::uint64_t hash) {
            std::ostringstream text;
            text << std::hex << std::setfill('0') << std::setw(16) << hash;
            return text.str();
        }

        constexpr std::string_view kMetricSetSql =
            "('partial_attention', 'spg', 'apg', 'apl', 'pending', 'pending_deadline')";

        void executeSql(sqlite3* db, const std::string& sql,
                        const std::atomic_bool* busyCancellationObserved) {
            char* error = nullptr;
            const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
            const bool canceled = result == SQLITE_INTERRUPT ||
                                  ((result == SQLITE_BUSY || result == SQLITE_LOCKED) &&
                                   busyCancellationObserved != nullptr &&
                                   busyCancellationObserved->load(std::memory_order_relaxed));
            if (canceled) {
                sqlite3_free(error);
                throw std::system_error(std::make_error_code(std::errc::operation_canceled),
                                        "activity analytics projection canceled");
            }
            if (result != SQLITE_OK) {
                const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
                sqlite3_free(error);
                throw ports::OperationError(
                    "Falha ao atualizar analises",
                    "activity analytics SQL failed: rc=" + std::to_string(result) +
                        " extended_rc=" + std::to_string(sqlite3_extended_errcode(db)) +
                        " message=" + message);
            }
            sqlite3_free(error);
        }

        void validateContext(const ActivityAnalyticsCaptureContext& context) {
            const domain::IsoWeek week{context.observedIsoYearWeek / 100,
                                       context.observedIsoYearWeek % 100};
            if (!domain::isValidIsoWeek(week)) {
                throw std::invalid_argument("analytics capture requires a valid ISO week");
            }
            const auto dateWeek = domain::isoWeekForDate(context.observedDate);
            if (!dateWeek.has_value()) {
                throw std::invalid_argument("analytics capture requires a canonical ISO date");
            }
            if (domain::toIsoYearWeek(*dateWeek) != context.observedIsoYearWeek) {
                throw std::invalid_argument(
                    "analytics capture date must belong to the observed ISO week");
            }
            if (context.sourceRevision.empty() || context.sourceFingerprint.empty()) {
                throw std::invalid_argument("analytics capture requires source identity");
            }
        }

        void ensureSchema(sqlite3* db, const std::atomic_bool* busyCancellationObserved) {
            executeSql(db,
                       "CREATE TABLE IF NOT EXISTS activity_analytics_meta("
                       "dataset TEXT PRIMARY KEY NOT NULL, schema_version INTEGER NOT NULL, "
                       "active_source_revision TEXT NOT NULL, baseline_iso_week INTEGER NOT NULL, "
                       "warning_window_days INTEGER CHECK(warning_window_days BETWEEN 0 AND 365))",
                       busyCancellationObserved);
            executeSql(
                db,
                "CREATE TABLE IF NOT EXISTS activity_analytics_snapshot("
                "dataset TEXT NOT NULL, observed_iso_week INTEGER NOT NULL, metric TEXT NOT NULL, "
                "source_revision TEXT NOT NULL, source_fingerprint TEXT NOT NULL, "
                "observed_date TEXT NOT NULL, captured_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
                "complete INTEGER NOT NULL CHECK(complete IN (0, 1)), reason TEXT NOT NULL, "
                "PRIMARY KEY(dataset, observed_iso_week, metric))",
                busyCancellationObserved);
            executeSql(
                db,
                "CREATE TABLE IF NOT EXISTS activity_analytics_point("
                "id INTEGER PRIMARY KEY, dataset TEXT NOT NULL, observed_iso_week INTEGER NOT "
                "NULL, "
                "metric TEXT NOT NULL, division TEXT NOT NULL, sector TEXT NOT NULL, "
                "person_role TEXT NOT NULL, person TEXT NOT NULL, registration_iso_week INTEGER, "
                "deadline_source_state TEXT NOT NULL, deadline_offset_days INTEGER, "
                "count INTEGER NOT NULL CHECK(count >= 0))",
                busyCancellationObserved);
            executeSql(db,
                       "CREATE INDEX IF NOT EXISTS idx_activity_analytics_point_lookup ON "
                       "activity_analytics_point(dataset, metric, observed_iso_week)",
                       busyCancellationObserved);
            executeSql(
                db,
                "CREATE INDEX IF NOT EXISTS idx_activity_analytics_point_dimensions ON "
                "activity_analytics_point(dataset, metric, division, sector, person_role, person)",
                busyCancellationObserved);
        }

        bool alreadyCaptured(sqlite3* db, const ActivityAnalyticsCaptureContext& context,
                             const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(
                db,
                "SELECT COUNT(*), COUNT(DISTINCT metric), "
                "COALESCE(SUM(CASE WHEN metric IN " +
                    std::string{kMetricSetSql} +
                    " THEN 1 ELSE 0 END), 0), "
                    "COALESCE(SUM(CASE WHEN source_revision=? AND source_fingerprint=? AND "
                    "observed_date=? "
                    "THEN 1 ELSE 0 END), 0), "
                    "COALESCE(SUM(CASE WHEN (metric='partial_attention' AND complete=0 AND "
                    "reason<>'') OR (metric IN ('spg','apg','apl','pending','pending_deadline') "
                    "AND complete=1 AND reason='') THEN 1 ELSE 0 END), 0), "
                    "COALESCE(SUM(CASE WHEN TYPEOF(dataset)='text' AND "
                    "TYPEOF(observed_iso_week)='integer' AND TYPEOF(metric)='text' AND "
                    "TYPEOF(source_revision)='text' AND TYPEOF(source_fingerprint)='text' AND "
                    "TYPEOF(observed_date)='text' AND TYPEOF(complete)='integer' AND "
                    "TYPEOF(reason)='text' THEN 1 ELSE 0 END), 0), "
                    "COALESCE((SELECT schema_version=? AND active_source_revision=(SELECT "
                    "source_revision FROM activity_analytics_snapshot WHERE dataset=? ORDER BY "
                    "observed_iso_week DESC, metric LIMIT 1) AND "
                    "baseline_iso_week=(SELECT MIN(observed_iso_week) FROM "
                    "activity_analytics_snapshot WHERE dataset=?) AND "
                    "(warning_window_days IS NULL OR (TYPEOF(warning_window_days)='integer' "
                    "AND warning_window_days BETWEEN 0 AND 365)) FROM activity_analytics_meta "
                    "WHERE dataset=?), "
                    "0) FROM activity_analytics_snapshot WHERE dataset=? AND observed_iso_week=?",
                busyCancellationObserved);
            statement.bindTextOneBased(1, context.sourceRevision);
            statement.bindTextOneBased(2, context.sourceFingerprint);
            statement.bindTextOneBased(3, context.observedDate);
            statement.bindInt64OneBased(4, kSchemaVersion);
            statement.bindTextOneBased(5, std::string{kDataset});
            statement.bindTextOneBased(6, std::string{kDataset});
            statement.bindTextOneBased(7, std::string{kDataset});
            statement.bindTextOneBased(8, std::string{kDataset});
            statement.bindInt64OneBased(9, context.observedIsoYearWeek);
            constexpr auto expectedMetricCount = static_cast<long long>(kMetrics.size());
            return statement.step() && statement.columnInt64(0) == expectedMetricCount &&
                   statement.columnInt64(1) == expectedMetricCount &&
                   statement.columnInt64(2) == expectedMetricCount &&
                   statement.columnInt64(3) == expectedMetricCount &&
                   statement.columnInt64(4) == expectedMetricCount &&
                   statement.columnInt64(5) == expectedMetricCount && statement.columnInt64(6) == 1;
        }

        void replaceWeek(sqlite3* db, const ActivityAnalyticsCaptureContext& context,
                         const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement deletePoints(
                db, "DELETE FROM activity_analytics_point WHERE dataset=? AND observed_iso_week=?",
                busyCancellationObserved);
            deletePoints.bindTextOneBased(1, std::string{kDataset});
            deletePoints.bindInt64OneBased(2, context.observedIsoYearWeek);
            deletePoints.executeAndReset();

            SqliteStatement deleteSnapshots(
                db,
                "DELETE FROM activity_analytics_snapshot WHERE dataset=? AND observed_iso_week=?",
                busyCancellationObserved);
            deleteSnapshots.bindTextOneBased(1, std::string{kDataset});
            deleteSnapshots.bindInt64OneBased(2, context.observedIsoYearWeek);
            deleteSnapshots.executeAndReset();
        }

        void writeMeta(sqlite3* db, const ActivityAnalyticsCaptureContext& context,
                       const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(
                db,
                "INSERT INTO activity_analytics_meta("
                "dataset, schema_version, active_source_revision, baseline_iso_week) "
                "VALUES(?, ?, ?, ?) ON CONFLICT(dataset) DO UPDATE SET "
                "schema_version=excluded.schema_version, "
                "active_source_revision=(SELECT source_revision FROM "
                "activity_analytics_snapshot WHERE dataset=excluded.dataset ORDER BY "
                "observed_iso_week DESC, metric LIMIT 1), "
                "baseline_iso_week=(SELECT MIN(observed_iso_week) FROM "
                "activity_analytics_snapshot WHERE dataset=excluded.dataset), "
                "warning_window_days=CASE WHEN TYPEOF(warning_window_days)='integer' AND "
                "warning_window_days BETWEEN 0 AND 365 THEN warning_window_days ELSE NULL END",
                busyCancellationObserved);
            statement.bindTextOneBased(1, std::string{kDataset});
            statement.bindInt64OneBased(2, kSchemaVersion);
            statement.bindTextOneBased(3, context.sourceRevision);
            statement.bindInt64OneBased(4, context.observedIsoYearWeek);
            statement.executeAndReset();
        }

        void writeSnapshot(sqlite3* db, const ActivityAnalyticsCaptureContext& context,
                           const StockMetric& metric,
                           const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(
                db,
                "INSERT INTO activity_analytics_snapshot("
                "dataset, observed_iso_week, metric, source_revision, source_fingerprint, "
                "observed_date, complete, reason) VALUES(?, ?, ?, ?, ?, ?, ?, ?)",
                busyCancellationObserved);
            statement.bindTextOneBased(1, std::string{kDataset});
            statement.bindInt64OneBased(2, context.observedIsoYearWeek);
            statement.bindTextOneBased(3, std::string{metric.key});
            statement.bindTextOneBased(4, context.sourceRevision);
            statement.bindTextOneBased(5, context.sourceFingerprint);
            statement.bindTextOneBased(6, context.observedDate);
            statement.bindInt64OneBased(7, metric.complete ? 1 : 0);
            statement.bindTextOneBased(8, std::string{metric.reason});
            statement.executeAndReset();
        }

        std::string pointInsertSql(const std::string& quotedTable, const StockMetric& metric,
                                   const PersonDimension& person) {
            const std::string deadlineState =
                metric.deadline ? "TRIM(COALESCE(status_execucao_prazo, ''))" : "''";
            const std::string deadlineOffset =
                metric.deadline
                    ? "CASE WHEN julianday(NULLIF(TRIM(COALESCE(prazo_limite, '')), '')) "
                      "IS NULL THEN NULL ELSE CAST(julianday(TRIM(prazo_limite)) - julianday(?) "
                      "AS INTEGER) END"
                    : "NULL";
            const auto registrationWeek = query::canonicalIsoWeekSqlExpression("semana_cadastro");
            return "INSERT INTO activity_analytics_point("
                   "dataset, observed_iso_week, metric, division, sector, person_role, person, "
                   "registration_iso_week, deadline_source_state, deadline_offset_days, count) "
                   "SELECT 'SSA', ?, '" +
                   std::string{metric.key} +
                   "', division, sector, ?, person, registration_iso_week, deadline_source_state, "
                   "deadline_offset_days, COUNT(DISTINCT numero_ssa) FROM (SELECT numero_ssa, "
                   "COALESCE(NULLIF(SUBSTR(UPPER(TRIM(setor_executor)), 1, 3), ''), "
                   "'Nao atribuido') AS division, "
                   "COALESCE(NULLIF(UPPER(TRIM(setor_executor)), ''), 'Nao atribuido') AS sector, "
                   "COALESCE(NULLIF(TRIM(" +
                   std::string{person.column} + "), ''), 'Nao atribuido') AS person, " +
                   registrationWeek + " AS registration_iso_week, " + deadlineState +
                   " AS deadline_source_state, " + deadlineOffset +
                   " AS deadline_offset_days FROM " + quotedTable +
                   " WHERE TRIM(COALESCE(numero_ssa, '')) <> '' AND (" +
                   std::string{metric.predicate} +
                   ")) GROUP BY division, sector, person, registration_iso_week, "
                   "deadline_source_state, deadline_offset_days";
        }

        void writePoints(sqlite3* db, const std::string& quotedTable,
                         const ActivityAnalyticsCaptureContext& context, const StockMetric& metric,
                         const PersonDimension& person,
                         const std::atomic_bool* busyCancellationObserved) {
            SqliteStatement statement(db, pointInsertSql(quotedTable, metric, person),
                                      busyCancellationObserved);
            statement.bindInt64OneBased(1, context.observedIsoYearWeek);
            statement.bindTextOneBased(2, std::string{person.role});
            if (metric.deadline) {
                statement.bindTextOneBased(3, context.observedDate);
            }
            statement.executeAndReset();
        }

    } // namespace

    ActivityAnalyticsCaptureResult
    SqliteActivityAnalyticsProjection::capture(sqlite3* db, const std::string_view sourceTable,
                                               const ActivityAnalyticsCaptureContext& context,
                                               const std::stop_token& stopToken,
                                               const std::atomic_bool* busyCancellationObserved) {
        if (db == nullptr) {
            throw std::invalid_argument("activity analytics projection requires a database");
        }
        const auto quotedTable = query::quoteTableIdentifier(std::string{sourceTable});
        validateContext(context);
        throwIfCanceled(stopToken);

        const bool ownsTransaction = sqlite3_get_autocommit(db) != 0;
        std::unique_ptr<SqliteWriteTransaction> transaction;
        if (ownsTransaction) {
            transaction = std::make_unique<SqliteWriteTransaction>(db, busyCancellationObserved);
        }
        SqliteStatement pointTable(db,
                                   "SELECT 1 FROM sqlite_master WHERE type='table' AND "
                                   "name='activity_analytics_point'",
                                   busyCancellationObserved);
        const bool pointTablePresent = pointTable.step();
        ensureSchema(db, busyCancellationObserved);
        throwIfCanceled(stopToken);
        if (pointTablePresent && alreadyCaptured(db, context, busyCancellationObserved)) {
            if (transaction) {
                transaction->commit();
            }
            return {false, static_cast<int>(kMetrics.size())};
        }

        replaceWeek(db, context, busyCancellationObserved);
        for (const auto& metric : kMetrics) {
            throwIfCanceled(stopToken);
            writeSnapshot(db, context, metric, busyCancellationObserved);
            if (!metric.complete) {
                continue;
            }
            for (const auto& person : kPeople) {
                throwIfCanceled(stopToken);
                writePoints(db, quotedTable, context, metric, person, busyCancellationObserved);
            }
        }
        writeMeta(db, context, busyCancellationObserved);
        if (transaction) {
            transaction->commit();
        }
        return {true, static_cast<int>(kMetrics.size())};
    }

    std::string
    SqliteActivityAnalyticsProjection::sourceFingerprint(std::vector<std::string> verifiedSources) {
        std::ranges::sort(verifiedSources);
        std::uint64_t hash = 14695981039346656037ULL;
        const auto appendByte = [&hash](const unsigned char byte) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        };
        const auto appendLength = [&appendByte](const std::uint64_t size) {
            for (int shift = 56; shift >= 0; shift -= 8) {
                appendByte(static_cast<unsigned char>(size >> shift));
            }
        };
        appendLength(verifiedSources.size());
        for (const auto& source : verifiedSources) {
            appendLength(source.size());
            for (const unsigned char byte : source) {
                appendByte(byte);
            }
        }
        return formatFingerprint(hash);
    }

    std::string SqliteActivityAnalyticsProjection::canonicalSourceFingerprint(
        sqlite3* db, const std::string_view sourceTable, const std::stop_token& stopToken,
        const std::atomic_bool* busyCancellationObserved) {
        if (db == nullptr) {
            throw std::invalid_argument("canonical analytics fingerprint requires a database");
        }
        throwIfCanceled(stopToken);

        std::ostringstream queryText;
        queryText << "SELECT ";
        for (std::size_t index = 0; index < kCanonicalSourceColumns.size(); ++index) {
            if (index > 0) {
                queryText << ", ";
            }
            queryText << query::quoteColumnIdentifier(std::string{kCanonicalSourceColumns[index]});
        }
        queryText << " FROM " << query::quoteTableIdentifier(std::string{sourceTable})
                  << " ORDER BY ";
        for (std::size_t index = 0; index < kCanonicalSourceColumns.size(); ++index) {
            if (index > 0) {
                queryText << ", ";
            }
            const auto column =
                query::quoteColumnIdentifier(std::string{kCanonicalSourceColumns[index]});
            queryText << "TYPEOF(" << column << ") COLLATE BINARY, " << column << " COLLATE BINARY";
        }

        std::uint64_t hash = 14695981039346656037ULL;
        const auto appendByte = [&hash](const unsigned char byte) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        };
        const auto appendLength = [&appendByte](const std::uint64_t size) {
            for (int shift = 56; shift >= 0; shift -= 8) {
                appendByte(static_cast<unsigned char>(size >> shift));
            }
        };
        const auto appendValue = [&appendByte, &appendLength](const int type,
                                                              const unsigned char* value,
                                                              const std::size_t size) {
            appendByte(static_cast<unsigned char>(type));
            appendLength(size);
            for (std::size_t index = 0; index < size; ++index) {
                appendByte(value[index]);
            }
        };
        constexpr std::string_view kFingerprintVersion = "ssa-analytics-canonical-v2";
        appendValue(SQLITE_TEXT, reinterpret_cast<const unsigned char*>(kFingerprintVersion.data()),
                    kFingerprintVersion.size());

        SqliteStatement statement(db, queryText.str(), busyCancellationObserved);
        while (statement.step()) {
            throwIfCanceled(stopToken);
            for (int column = 0; column < statement.columnCount(); ++column) {
                const int type = sqlite3_column_type(statement.handle(), column);
                if (type == SQLITE_NULL) {
                    appendValue(type, nullptr, 0);
                    continue;
                }
                if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) {
                    const std::uint64_t bits =
                        type == SQLITE_INTEGER
                            ? static_cast<std::uint64_t>(
                                  sqlite3_column_int64(statement.handle(), column))
                            : std::bit_cast<std::uint64_t>(
                                  sqlite3_column_double(statement.handle(), column));
                    std::array<unsigned char, sizeof(bits)> encoded{};
                    for (std::size_t index = 0; index < encoded.size(); ++index) {
                        const auto shift = static_cast<int>((encoded.size() - index - 1) * 8);
                        encoded[index] = static_cast<unsigned char>(bits >> shift);
                    }
                    appendValue(type, encoded.data(), encoded.size());
                    continue;
                }
                const int byteCount = sqlite3_column_bytes(statement.handle(), column);
                const auto* value = type == SQLITE_TEXT
                                        ? sqlite3_column_text(statement.handle(), column)
                                        : static_cast<const unsigned char*>(
                                              sqlite3_column_blob(statement.handle(), column));
                appendValue(type, value, static_cast<std::size_t>(byteCount));
            }
            appendByte(0xffU);
        }
        throwIfCanceled(stopToken);
        return formatFingerprint(hash);
    }

} // namespace ssa::infra::sqlite
