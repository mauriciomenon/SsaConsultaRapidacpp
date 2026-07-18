#include "infra/sqlite/SqliteSsaRepository.h"

#include "infra/sqlite/SqliteDatabaseWriteLock.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/OperationError.h"
#include "query/SqlQueryText.h"

#include <iostream>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <utility>

namespace ssa::infra::sqlite {

    namespace {

        bool requestUsesDerivedCount(const domain::SsaPageRequest& request) {
            return std::ranges::any_of(request.visibleColumns,
                                       domain::ColumnCatalog::isDerivedCountColumn) ||
                   domain::ColumnCatalog::isDerivedCountColumn(request.sort.columnKey) ||
                   request.visibleColumns.empty();
        }

        bool isReadonlyDiagnostic(const std::string_view diagnostic) {
            return diagnostic.find("readonly") != std::string_view::npos ||
                   diagnostic.find("read-only") != std::string_view::npos;
        }

        bool hasDerivedCountColumns(sqlite3* db, const std::string& tableName,
                                    const std::atomic_bool* busyCanceled) {
            SqliteStatement statement(
                db, "PRAGMA table_info(" + query::quoteTableIdentifier(tableName) + ")",
                busyCanceled);
            bool hasNumber = false;
            bool hasDerivation = false;
            while (statement.step()) {
                const auto name = statement.columnText(1);
                hasNumber = hasNumber || name == domain::kSsaNumberColumnKey;
                hasDerivation = hasDerivation || name == "derivada_de";
            }
            return hasNumber && hasDerivation;
        }

        void bindAll(SqliteStatement& statement, const std::vector<std::string>& bindings) {
            int index = 1;
            for (const auto& binding : bindings) {
                statement.bindTextOneBased(index, binding);
                ++index;
            }
        }

        std::shared_ptr<const domain::SsaRecord::SchemaIndex>
        readSchema(SqliteStatement& statement) {
            const int count = statement.columnCount();
            auto schema = std::make_shared<domain::SsaRecord::SchemaIndex>();
            schema->keys.reserve(static_cast<std::size_t>(count));
            schema->indexByKey.reserve(static_cast<std::size_t>(count));
            for (int column = 0; column < count; ++column) {
                const auto key = statement.columnName(column);
                schema->indexByKey.emplace(key, static_cast<std::size_t>(column));
                schema->keys.push_back(key);
            }
            return schema;
        }

        domain::SsaRecord readRecord(SqliteStatement& statement,
                                     std::shared_ptr<const domain::SsaRecord::SchemaIndex> schema) {
            std::vector<std::string> values(schema->keys.size());
            for (int column = 0; column < statement.columnCount(); ++column) {
                values[static_cast<std::size_t>(column)] = statement.columnText(column);
            }
            return domain::SsaRecord{std::move(schema), std::move(values)};
        }

    } // namespace

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath)
        : SqliteSsaRepository(std::move(dbPath), query::SqlQueryBuilder{}) {}

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath,
                                             query::SqlQueryBuilder queryBuilder)
        : SqliteSsaRepository(std::move(dbPath), std::move(queryBuilder),
                              [] { return std::chrono::steady_clock::now(); }, {}) {}

    SqliteSsaRepository::SqliteSsaRepository(std::filesystem::path dbPath,
                                             query::SqlQueryBuilder queryBuilder,
                                             DerivedCountCooldownNow now)
        : SqliteSsaRepository(std::move(dbPath), std::move(queryBuilder), std::move(now), {}) {}

    SqliteSsaRepository::SqliteSsaRepository(
        std::filesystem::path dbPath, query::SqlQueryBuilder queryBuilder,
        DerivedCountCooldownNow now,
        const SynchronizationSignals& synchronization) // NOLINT(modernize-pass-by-value)
        : dbPath_(std::move(dbPath)), queryBuilder_(std::move(queryBuilder)), now_(std::move(now)),
          synchronization_(synchronization) {}

    bool SqliteSsaRepository::ensureDerivedCountSummary(const std::stop_token stopToken) const {
        throwIfCanceled(stopToken);
        {
            std::unique_lock guard(derivedCountSummaryMutex_);
            if (derivedCountSummaryAvailable_) {
                return true;
            }
            if (derivedCountSummaryUnsupported_ || derivedCountSummaryInitializing_ ||
                now_() < derivedCountSummaryRetryAfter_) {
                return false;
            }
            derivedCountSummaryInitializing_ = true;
        }

        try {
            const SqliteDatabaseWriteLock writeLock(dbPath_);
            if (!writeLock.acquired()) {
                const auto failure = writeLock.error() == QLockFile::LockFailedError
                                         ? DerivedCountSummaryFailure::LockHeld
                                         : DerivedCountSummaryFailure::LockError;
                bool shouldLog = false;
                {
                    std::unique_lock guard(derivedCountSummaryMutex_);
                    shouldLog = derivedCountSummaryFailure_ != failure;
                    derivedCountSummaryFailure_ = failure;
                    derivedCountSummaryRetryAfter_ = now_() + std::chrono::seconds{1};
                    derivedCountSummaryInitializing_ = false;
                }
                if (shouldLog) {
                    std::clog << "sqlite derived count summary unavailable while database write "
                                 "lock cannot be acquired: "
                              << writeLock.diagnostic() << '\n';
                }
                return false;
            }
            if (synchronization_.derivedCountWriteLockAcquired != nullptr) {
                synchronization_.derivedCountWriteLockAcquired->release();
            }
            SqliteConnection sqlite(dbPath_, SqliteOpenMode::ReadWrite);
            SqliteBusyHandler busy(sqlite.handle(), stopToken, std::chrono::milliseconds{3000},
                                   synchronization_.busyEntered.get());
            SqliteProgressHandler progress(sqlite.handle(), stopToken);
            if (!hasDerivedCountColumns(sqlite.handle(), queryBuilder_.rawTableName(),
                                        busy.cancellationObserved())) {
                std::unique_lock guard(derivedCountSummaryMutex_);
                derivedCountSummaryUnsupported_ = true;
                derivedCountSummaryInitializing_ = false;
                return false;
            }
            SqliteWriteTransaction transaction(sqlite.handle(), busy.cancellationObserved());
            ssa::infra::sqlite::ensureDerivedCountSummary(
                sqlite.handle(), queryBuilder_.rawTableName(),
                domain::ColumnCatalog::schemaColumns(), busy.cancellationObserved());
            transaction.commit();
            {
                std::unique_lock guard(derivedCountSummaryMutex_);
                derivedCountSummaryAvailable_ = true;
                derivedCountSummaryFailure_ = DerivedCountSummaryFailure::None;
                derivedCountSummaryRetryAfter_ = {};
                derivedCountSummaryInitializing_ = false;
            }
            return true;
        } catch (const ports::OperationError& error) {
            if (!isReadonlyDiagnostic(error.diagnostic())) {
                std::unique_lock guard(derivedCountSummaryMutex_);
                derivedCountSummaryInitializing_ = false;
                throw;
            }
            bool shouldLog = false;
            {
                std::unique_lock guard(derivedCountSummaryMutex_);
                shouldLog = derivedCountSummaryFailure_ != DerivedCountSummaryFailure::Readonly;
                derivedCountSummaryFailure_ = DerivedCountSummaryFailure::Readonly;
                derivedCountSummaryRetryAfter_ = now_() + std::chrono::seconds{1};
                derivedCountSummaryInitializing_ = false;
            }
            if (shouldLog) {
                std::clog << "sqlite derived count summary unavailable in readonly database: "
                          << error.diagnostic() << '\n';
            }
            return false;
        } catch (const std::runtime_error& error) {
            if (!isReadonlyDiagnostic(error.what())) {
                std::unique_lock guard(derivedCountSummaryMutex_);
                derivedCountSummaryInitializing_ = false;
                throw;
            }
            bool shouldLog = false;
            {
                std::unique_lock guard(derivedCountSummaryMutex_);
                shouldLog = derivedCountSummaryFailure_ != DerivedCountSummaryFailure::Readonly;
                derivedCountSummaryFailure_ = DerivedCountSummaryFailure::Readonly;
                derivedCountSummaryRetryAfter_ = now_() + std::chrono::seconds{1};
                derivedCountSummaryInitializing_ = false;
            }
            if (shouldLog) {
                std::clog << "sqlite derived count summary unavailable in readonly database: "
                          << error.what() << '\n';
            }
            return false;
        } catch (...) {
            std::unique_lock guard(derivedCountSummaryMutex_);
            derivedCountSummaryInitializing_ = false;
            throw;
        }
    }

    domain::SsaPageResult SqliteSsaRepository::page(const domain::SsaPageRequest& request,
                                                    std::stop_token stopToken) const {
        const bool usesDerivedCount =
            requestUsesDerivedCount(request) && ensureDerivedCountSummary(stopToken);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken, std::chrono::milliseconds{3000},
                               synchronization_.busyEntered.get());
        SqliteProgressHandler progress(sqlite.handle(), stopToken,
                                       synchronization_.progressEntered.get());
        const auto queries = queryBuilder_.build(request, usesDerivedCount);
        SqliteReadTransaction transaction(sqlite.handle(), stopToken, busy.cancellationObserved());

        domain::SsaPageResult result;
        result.totalRows =
            executeCount(sqlite.handle(), queries.count, stopToken, busy.cancellationObserved());
        result.pageIndex = request.pageIndex;
        result.pageSize = request.pageSize;
        result.rows =
            executeRows(sqlite.handle(), queries.page, stopToken, busy.cancellationObserved());
        transaction.commit();

        return result;
    }

    std::size_t SqliteSsaRepository::count(const domain::SsaPageRequest& request,
                                           std::stop_token stopToken) const {
        const auto countQuery = queryBuilder_.buildCount(request);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        return executeCount(sqlite.handle(), countQuery, stopToken, busy.cancellationObserved());
    }

    std::optional<domain::SsaRecord>
    SqliteSsaRepository::recordBySsaNumber(const domain::SsaNumber& number,
                                           std::stop_token stopToken) const {
        const auto query = queryBuilder_.buildRecordBySsaNumber(number);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        SqliteStatement statement(sqlite.handle(), query.record.sql, busy.cancellationObserved());
        bindAll(statement, query.record.bindings);
        if (!statement.step()) {
            return std::nullopt;
        }
        throwIfCanceled(stopToken);
        return readRecord(statement, readSchema(statement));
    }

    std::vector<std::string>
    SqliteSsaRepository::distinctValues(const domain::DistinctValuesRequest& request,
                                        std::stop_token stopToken) const {
        const auto query = queryBuilder_.buildDistinctValues(request);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        SqliteStatement statement(sqlite.handle(), query.sql, busy.cancellationObserved());
        bindAll(statement, query.bindings);

        std::vector<std::string> values;
        while (statement.step()) {
            throwIfCanceled(stopToken);
            const auto value = statement.columnText(0);
            if (!value.empty()) {
                values.push_back(value);
            }
        }
        return values;
    }

    std::size_t SqliteSsaRepository::maxValueLength(const std::string_view columnKey,
                                                    std::stop_token stopToken) const {
        const auto query = queryBuilder_.buildMaxValueLength(columnKey);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        return executeCount(sqlite.handle(), query, stopToken, busy.cancellationObserved());
    }

    std::vector<domain::SsaDerivadaEntry>
    SqliteSsaRepository::derivadasDiretas(const domain::SsaNumber& number,
                                          std::stop_token stopToken) const {
        const auto query = queryBuilder_.buildDirectDerivations(number);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        SqliteStatement statement(sqlite.handle(), query.sql, busy.cancellationObserved());
        bindAll(statement, query.bindings);
        std::vector<domain::SsaDerivadaEntry> entries;
        while (statement.step()) {
            throwIfCanceled(stopToken);
            domain::SsaDerivadaEntry entry;
            entry.number = statement.columnText(0);
            entry.situacao = statement.columnText(1);
            if (!entry.number.empty()) {
                entries.push_back(std::move(entry));
            }
        }
        return entries;
    }

    ports::SsaReadResult SqliteSsaRepository::readAll(const domain::SsaPageRequest& request,
                                                      ports::SsaRecordConsumer consume,
                                                      std::stop_token stopToken) const {
        const bool usesDerivedCount =
            requestUsesDerivedCount(request) && ensureDerivedCountSummary(stopToken);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken, std::chrono::milliseconds{3000},
                               synchronization_.busyEntered.get());
        SqliteProgressHandler progress(sqlite.handle(), stopToken,
                                       synchronization_.progressEntered.get());
        SqliteReadTransaction transaction(sqlite.handle(), stopToken, busy.cancellationObserved());
        // pageSize == 0 means unbounded streaming (single query, no LIMIT).
        // pageSize > 0 means paginated streaming: read in chunks so peak memory
        // stays bounded to one page even for large filtered result sets.
        if (request.pageSize == 0) {
            const auto query = queryBuilder_.buildRows(request, usesDerivedCount);
            const auto result = consumeRows(sqlite.handle(), query, consume, stopToken,
                                            busy.cancellationObserved());
            if (result.ok()) {
                transaction.commit();
            }
            return result;
        }
        std::size_t rowCount = 0;
        for (std::size_t pageIndex = 0;; ++pageIndex) {
            throwIfCanceled(stopToken);
            auto paged = request;
            paged.pageIndex = pageIndex;
            const auto query = queryBuilder_.buildRows(paged, usesDerivedCount);
            std::size_t before = rowCount;
            const auto result = consumeRows(
                sqlite.handle(), query,
                [&](const domain::SsaRecord& row) {
                    ++rowCount;
                    return consume(row);
                },
                stopToken, busy.cancellationObserved());
            if (!result.ok()) {
                return {rowCount, result.error};
            }
            const std::size_t emitted = rowCount - before;
            if (emitted < request.pageSize) {
                break;
            }
        }
        transaction.commit();
        return {rowCount, {}};
    }

    std::vector<domain::SsaExecutadasReportRow>
    SqliteSsaRepository::executadasReport(const domain::SsaPageRequest& request,
                                          const bool byDivision,
                                          const std::stop_token stopToken) const {
        const auto query = queryBuilder_.buildExecutadasReport(request, byDivision);
        SqliteConnection sqlite(dbPath_);
        SqliteBusyHandler busy(sqlite.handle(), stopToken);
        SqliteProgressHandler progress(sqlite.handle(), stopToken);
        SqliteStatement statement(sqlite.handle(), query.sql, busy.cancellationObserved());
        bindAll(statement, query.bindings);

        std::vector<domain::SsaExecutadasReportRow> rows;
        while (statement.step()) {
            throwIfCanceled(stopToken);
            rows.push_back(domain::SsaExecutadasReportRow{
                statement.columnText(0), statement.columnText(1), statement.columnText(2),
                static_cast<int>(statement.columnInt64(3))});
        }
        return rows;
    }

    std::size_t SqliteSsaRepository::executeCount(sqlite3* db, const query::SqlQuery& query,
                                                  const std::stop_token& stopToken,
                                                  const std::atomic_bool* busyCanceled) {
        SqliteStatement statement(db, query.sql, busyCanceled);
        bindAll(statement, query.bindings);
        if (!statement.step()) {
            throw std::runtime_error("sqlite count query did not return a result");
        }
        return static_cast<std::size_t>(statement.columnInt64(0));
    }

    std::vector<domain::SsaRecord>
    SqliteSsaRepository::executeRows(sqlite3* db, const query::SqlQuery& query,
                                     const std::stop_token& stopToken,
                                     const std::atomic_bool* busyCanceled) {
        std::vector<domain::SsaRecord> rows;
        const auto result = consumeRows(
            db, query,
            [&rows](const domain::SsaRecord& row) {
                rows.push_back(row);
                return std::nullopt;
            },
            stopToken, busyCanceled);
        if (!result.ok()) {
            throw std::runtime_error(result.error);
        }
        return rows;
    }

    ports::SsaReadResult SqliteSsaRepository::consumeRows(sqlite3* db, const query::SqlQuery& query,
                                                          const ports::SsaRecordConsumer& consume,
                                                          const std::stop_token& stopToken,
                                                          const std::atomic_bool* busyCanceled) {
        SqliteStatement statement(db, query.sql, busyCanceled);
        bindAll(statement, query.bindings);

        std::size_t rowCount = 0;
        std::shared_ptr<const domain::SsaRecord::SchemaIndex> schema;
        while (statement.step()) {
            throwIfCanceled(stopToken);
            if (!schema) {
                schema = readSchema(statement);
            }
            auto record = readRecord(statement, schema);
            ++rowCount;
            if (auto error = consume(record); error.has_value()) {
                return {rowCount, *error};
            }
            throwIfCanceled(stopToken);
        }
        return {rowCount, {}};
    }

} // namespace ssa::infra::sqlite
