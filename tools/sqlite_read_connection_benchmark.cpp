#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "qt/FilesystemPath.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>

#include <sqlite3.h>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#else
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <latch>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

    constexpr std::size_t kDefaultRows = 250'000;
    constexpr std::size_t kMaxRows = 299'999'999;
    constexpr int kDefaultSamples = 1;
    constexpr int kMaxSamples = 1'000;
    constexpr int kDefaultOperations = 100;
    constexpr int kMaxOperations = 10'000;
    constexpr int kDefaultThreads = 4;
    constexpr int kMaxThreads = 64;
    constexpr int kDefaultReadsPerThread = 20;
    constexpr int kMaxReadsPerThread = 10'000;
    constexpr std::size_t kMaxConcurrentReads = 1'000'000;
    constexpr std::size_t kMaxTotalMeasuredOperations = 100'000;
    constexpr std::size_t kMaxTotalConcurrentReads = 100'000;
    constexpr int kBurnInOperations = 10;
    constexpr std::int64_t kFirstSsaNumber = 700'000'000;
    constexpr int kWorkerTimeoutMs = 60'000;
    constexpr std::string_view kStatusLastQuery =
        "SELECT numero_ssa, descricao_ssa, situacao FROM ssa_table ORDER BY CASE WHEN "
        "UPPER(COALESCE(situacao, '')) <> 'STE' THEN 0 ELSE 1 END ASC, numero_ssa DESC LIMIT 50";
    constexpr std::string_view kStatusLastIndexName = "idx_ssa_table_status_last_numero_ssa_desc";
    constexpr std::string_view kStatusLastIndexSql =
        "CREATE INDEX idx_ssa_table_status_last_numero_ssa_desc ON ssa_table(CASE WHEN "
        "UPPER(COALESCE(situacao, '')) <> 'STE' THEN 0 ELSE 1 END ASC, numero_ssa DESC)";

    struct Metric final {
        double wallMs{0.0};
        double cpuMs{0.0};
        std::uint64_t rssBefore{0};
        std::uint64_t rssAfter{0};
        std::int64_t rssDelta{0};
    };

    struct BenchmarkWorkload final {
        std::size_t rows{0};
        int operations{0};
        int threads{0};
        int readsPerThread{0};
    };

    using StatusLastRows = std::vector<std::array<std::string, 3>>;

    struct StatusLastVariant final {
        std::vector<Metric> metrics;
        std::vector<StatusLastRows> rows;
    };

    [[nodiscard]] std::string canonicalNumber(const std::size_t offset) {
        return std::to_string(kFirstSsaNumber + static_cast<std::int64_t>(offset));
    }

    [[nodiscard]] std::optional<std::uint64_t> processCpuNanoseconds() {
#ifdef _WIN32
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0) {
            return std::nullopt;
        }
        ULARGE_INTEGER kernelRaw{};
        kernelRaw.LowPart = kernel.dwLowDateTime;
        kernelRaw.HighPart = kernel.dwHighDateTime;
        ULARGE_INTEGER userRaw{};
        userRaw.LowPart = user.dwLowDateTime;
        userRaw.HighPart = user.dwHighDateTime;
        return (kernelRaw.QuadPart + userRaw.QuadPart) * 100ULL;
#else
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return std::nullopt;
        }
        const auto toNanoseconds = [](const timeval value) {
            return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL +
                   static_cast<std::uint64_t>(value.tv_usec) * 1'000ULL;
        };
        return toNanoseconds(usage.ru_utime) + toNanoseconds(usage.ru_stime);
#endif
    }

    [[nodiscard]] std::uint64_t currentRssBytes() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS info{};
        info.cb = sizeof(info);
        return GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)) == 0
                   ? 0
                   : static_cast<std::uint64_t>(info.WorkingSetSize);
#elif defined(__APPLE__)
        mach_task_basic_info_data_t info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        return task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                         reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS
                   ? static_cast<std::uint64_t>(info.resident_size)
                   : 0;
#else
        FILE* status = std::fopen("/proc/self/statm", "r");
        if (status == nullptr) {
            return 0;
        }
        unsigned long totalPages = 0;
        unsigned long residentPages = 0;
        const int matched = std::fscanf(status, "%lu %lu", &totalPages, &residentPages);
        std::fclose(status);
        const long pageSize = sysconf(_SC_PAGESIZE);
        return matched == 2 && pageSize > 0 ? residentPages * static_cast<std::uint64_t>(pageSize)
                                            : 0;
#endif
    }

    template <typename Operation> [[nodiscard]] auto measure(Operation&& operation) {
        using Result = std::invoke_result_t<Operation>;
        const auto rssBefore = currentRssBytes();
        const auto cpuBefore = processCpuNanoseconds();
        const auto started = std::chrono::steady_clock::now();
        Result result = std::forward<Operation>(operation)();
        const auto finished = std::chrono::steady_clock::now();
        const auto cpuAfter = processCpuNanoseconds();
        const auto rssAfter = currentRssBytes();
        if (!cpuBefore || !cpuAfter || *cpuAfter < *cpuBefore || rssBefore == 0 || rssAfter == 0) {
            throw std::runtime_error("connection benchmark metrics unavailable");
        }
        return std::pair<Result, Metric>{
            std::move(result),
            {std::chrono::duration<double, std::milli>(finished - started).count(),
             static_cast<double>(*cpuAfter - *cpuBefore) / 1'000'000.0, rssBefore, rssAfter,
             static_cast<std::int64_t>(rssAfter) - static_cast<std::int64_t>(rssBefore)}};
    }

    [[nodiscard]] QJsonObject metricJson(const Metric& metric) {
        return {{QStringLiteral("wall_ms"), metric.wallMs},
                {QStringLiteral("cpu_ms"), metric.cpuMs},
                {QStringLiteral("rss_before_bytes"), static_cast<qint64>(metric.rssBefore)},
                {QStringLiteral("rss_after_bytes"), static_cast<qint64>(metric.rssAfter)},
                {QStringLiteral("rss_delta_bytes"), static_cast<qint64>(metric.rssDelta)}};
    }

    [[nodiscard]] bool execute(sqlite3* database, const std::string& sql, std::string& error) {
        char* sqliteError = nullptr;
        const int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &sqliteError);
        if (result == SQLITE_OK) {
            return true;
        }
        error = sqliteError == nullptr ? sqlite3_errmsg(database) : sqliteError;
        sqlite3_free(sqliteError);
        return false;
    }

    [[nodiscard]] std::string createTableSql() {
        std::string sql = "CREATE TABLE ssa_table (";
        bool first = true;
        for (const auto& column : ssa::domain::ColumnCatalog::all()) {
            if (!first) {
                sql += ", ";
            }
            sql += column.key;
            sql += column.key == "id"                                ? " INTEGER PRIMARY KEY"
                   : column.type == ssa::domain::ColumnType::Integer ? " INTEGER"
                                                                     : " TEXT";
            first = false;
        }
        return sql + ")";
    }

    [[nodiscard]] bool createFixture(const std::filesystem::path& path, const std::size_t rows,
                                     std::string& error) {
        sqlite3* database = nullptr;
        const auto encodedPath = ssa::qt::toUtf8(path);
        if (sqlite3_open_v2(encodedPath.c_str(), &database,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            error =
                database == nullptr ? "cannot open benchmark fixture" : sqlite3_errmsg(database);
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return false;
        }
        const auto closeDatabase = [&] {
            if (sqlite3_close(database) == SQLITE_OK) {
                return true;
            }
            if (error.empty()) {
                error = "cannot close benchmark fixture";
            }
            return false;
        };
        if (!execute(database, createTableSql(), error)) {
            static_cast<void>(closeDatabase());
            return false;
        }
        if (!execute(database, "BEGIN IMMEDIATE", error)) {
            static_cast<void>(closeDatabase());
            return false;
        }
        const auto rollbackAndClose = [&] {
            std::string rollbackError;
            if (!execute(database, "ROLLBACK", rollbackError) && error.empty()) {
                error = std::move(rollbackError);
            }
            static_cast<void>(closeDatabase());
            return false;
        };
        sqlite3_stmt* insert = nullptr;
        const char* sql =
            "INSERT INTO ssa_table(numero_ssa, descricao_ssa, situacao) VALUES(?, ?, ?)";
        if (sqlite3_prepare_v2(database, sql, -1, &insert, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(database);
            return rollbackAndClose();
        }
        bool inserted = true;
        for (std::size_t index = 0; index < rows; ++index) {
            const auto number = canonicalNumber(index);
            const auto description = "Fixture " + number;
            const char* const status = index % 5 == 0 ? "STE" : "APV";
            inserted =
                sqlite3_bind_text(insert, 1, number.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
                sqlite3_bind_text(insert, 2, description.c_str(), -1, SQLITE_TRANSIENT) ==
                    SQLITE_OK &&
                sqlite3_bind_text(insert, 3, status, -1, SQLITE_STATIC) == SQLITE_OK &&
                sqlite3_step(insert) == SQLITE_DONE;
            if (!inserted) {
                error = sqlite3_errmsg(database);
                break;
            }
            sqlite3_reset(insert);
            sqlite3_clear_bindings(insert);
        }
        const int finalizeResult = sqlite3_finalize(insert);
        if (!inserted || finalizeResult != SQLITE_OK) {
            if (error.empty()) {
                error = sqlite3_errmsg(database);
            }
            return rollbackAndClose();
        }
        if (!execute(database, "COMMIT", error)) {
            return rollbackAndClose();
        }
        const bool indexed =
            execute(database,
                    "CREATE UNIQUE INDEX ux_ssa_table_numero_ssa ON ssa_table(numero_ssa)", error);
        const bool closed = closeDatabase();
        return indexed && closed;
    }

    [[nodiscard]] bool recordMatches(const std::optional<ssa::domain::SsaRecord>& record,
                                     const std::string& number) {
        return record.has_value() && record->valueOf("numero_ssa") == number;
    }

    [[nodiscard]] ssa::domain::SsaPageRequest pageRequest(const std::string& number) {
        ssa::domain::SsaPageRequest request;
        request.pageSize = 10;
        request.visibleColumns = {"numero_ssa", "descricao_ssa"};
        request.columnFilters = {{"numero_ssa", "=" + number}};
        request.excludeScaSesSte = false;
        return request;
    }

    [[nodiscard]] QJsonObject runWorker(const std::filesystem::path& databasePath,
                                        const BenchmarkWorkload& workload) {
        const auto [rows, operations, threads, readsPerThread] = workload;
        ssa::infra::sqlite::SqliteSsaRepository repository(databasePath);
        const auto firstNumber = canonicalNumber(0);
        std::stop_source firstStop;
        auto [firstRecord, firstMetric] = measure([&] {
            return repository.recordBySsaNumber(ssa::domain::SsaNumber{firstNumber},
                                                firstStop.get_token());
        });
        if (!recordMatches(firstRecord, firstNumber)) {
            throw std::runtime_error("fresh_process_open result contract failed");
        }

        for (int index = 0; index < kBurnInOperations; ++index) {
            std::stop_source stop;
            if (!recordMatches(
                    repository.recordBySsaNumber(ssa::domain::SsaNumber{canonicalNumber(
                                                     static_cast<std::size_t>(index) % rows)},
                                                 stop.get_token()),
                    canonicalNumber(static_cast<std::size_t>(index) % rows))) {
                throw std::runtime_error("repeated_open burn-in result contract failed");
            }
        }
        QJsonArray repeatedOperations;
        for (int index = 0; index < operations; ++index) {
            const auto number = canonicalNumber(static_cast<std::size_t>(index) % rows);
            std::stop_source stop;
            auto [record, metric] = measure([&] {
                return repository.recordBySsaNumber(ssa::domain::SsaNumber{number},
                                                    stop.get_token());
            });
            if (!recordMatches(record, number)) {
                throw std::runtime_error("repeated_open result contract failed");
            }
            repeatedOperations.append(metricJson(metric));
        }

        std::latch ready(threads);
        std::latch start(1);
        std::mutex latencyMutex;
        std::vector<double> latencies;
        const auto expectedReads =
            static_cast<std::size_t>(threads) * static_cast<std::size_t>(readsPerThread);
        latencies.reserve(expectedReads);
        std::mutex errorMutex;
        std::exception_ptr threadError;
        std::vector<std::jthread> workers;
        workers.reserve(static_cast<std::size_t>(threads));
        const auto concurrentRssBefore = currentRssBytes();
        try {
            for (int threadIndex = 0; threadIndex < threads; ++threadIndex) {
                workers.emplace_back([&, threadIndex] {
                    ready.count_down();
                    start.wait();
                    try {
                        for (int readIndex = 0; readIndex < readsPerThread; ++readIndex) {
                            const auto offset = (static_cast<std::size_t>(threadIndex) *
                                                     static_cast<std::size_t>(readsPerThread) +
                                                 static_cast<std::size_t>(readIndex)) %
                                                rows;
                            const auto number = canonicalNumber(offset);
                            std::stop_source stop;
                            const auto began = std::chrono::steady_clock::now();
                            const auto page =
                                repository.page(pageRequest(number), stop.get_token());
                            const auto ended = std::chrono::steady_clock::now();
                            if (page.totalRows != 1 || page.rows.size() != 1 ||
                                page.rows.front().valueOf("numero_ssa") != number) {
                                throw std::runtime_error(
                                    "concurrent_keystrokes result contract failed");
                            }
                            const double latency =
                                std::chrono::duration<double, std::milli>(ended - began).count();
                            const std::scoped_lock lock(latencyMutex);
                            latencies.push_back(latency);
                        }
                    } catch (...) {
                        const std::scoped_lock lock(errorMutex);
                        if (!threadError) {
                            threadError = std::current_exception();
                        }
                    }
                });
            }
        } catch (...) {
            start.count_down();
            for (auto& worker : workers) {
                worker.join();
            }
            throw;
        }
        ready.wait();
        const auto concurrentCpuBefore = processCpuNanoseconds();
        const auto concurrentStarted = std::chrono::steady_clock::now();
        start.count_down();
        for (auto& worker : workers) {
            worker.join();
        }
        const auto concurrentFinished = std::chrono::steady_clock::now();
        const auto concurrentCpuAfter = processCpuNanoseconds();
        const auto concurrentRssAfter = currentRssBytes();
        if (threadError) {
            std::rethrow_exception(threadError);
        }
        if (!concurrentCpuBefore || !concurrentCpuAfter ||
            *concurrentCpuAfter < *concurrentCpuBefore || concurrentRssBefore == 0 ||
            concurrentRssAfter == 0 || latencies.size() != expectedReads) {
            throw std::runtime_error("concurrent_keystrokes metrics contract failed");
        }
        QJsonArray concurrentLatencies;
        for (const double latency : latencies) {
            concurrentLatencies.append(latency);
        }
        const Metric concurrentMetric{
            std::chrono::duration<double, std::milli>(concurrentFinished - concurrentStarted)
                .count(),
            static_cast<double>(*concurrentCpuAfter - *concurrentCpuBefore) / 1'000'000.0,
            concurrentRssBefore, concurrentRssAfter,
            static_cast<std::int64_t>(concurrentRssAfter) -
                static_cast<std::int64_t>(concurrentRssBefore)};
        return {{QStringLiteral("fresh_process_open"), metricJson(firstMetric)},
                {QStringLiteral("repeated_open"),
                 QJsonObject{{QStringLiteral("burn_in_operations"), kBurnInOperations},
                             {QStringLiteral("operations"), repeatedOperations}}},
                {QStringLiteral("concurrent_keystrokes"),
                 QJsonObject{
                     {QStringLiteral("threads"), threads},
                     {QStringLiteral("reads_per_thread"), readsPerThread},
                     {QStringLiteral("requested_reads"), static_cast<qint64>(expectedReads)},
                     {QStringLiteral("successful_reads"), static_cast<qint64>(latencies.size())},
                     {QStringLiteral("failed_reads"), 0},
                     {QStringLiteral("canceled_reads"), 0},
                     {QStringLiteral("rss_scope"),
                      QStringLiteral("before_worker_creation_to_after_join_before_destruction")},
                     {QStringLiteral("batch"), metricJson(concurrentMetric)},
                     {QStringLiteral("read_wall_ms"), concurrentLatencies}}}};
    }

    [[nodiscard]] double percentile(std::vector<double> values, const double fraction) {
        std::sort(values.begin(), values.end());
        const auto index =
            static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * fraction)) - 1;
        return values[index];
    }

    [[nodiscard]] QJsonObject summary(const std::vector<double>& values) {
        return {{QStringLiteral("observation_count"), static_cast<qint64>(values.size())},
                {QStringLiteral("p50"), percentile(values, 0.50)},
                {QStringLiteral("p95"), percentile(values, 0.95)}};
    }

    [[nodiscard]] bool metricObjectValid(const QJsonObject& object) {
        const auto wall = object.value(QStringLiteral("wall_ms"));
        const auto cpu = object.value(QStringLiteral("cpu_ms"));
        const auto rssBefore = object.value(QStringLiteral("rss_before_bytes"));
        const auto rssAfter = object.value(QStringLiteral("rss_after_bytes"));
        const auto rssDelta = object.value(QStringLiteral("rss_delta_bytes"));
        return wall.isDouble() && std::isfinite(wall.toDouble()) && wall.toDouble() >= 0.0 &&
               cpu.isDouble() && std::isfinite(cpu.toDouble()) && cpu.toDouble() >= 0.0 &&
               rssBefore.isDouble() && std::isfinite(rssBefore.toDouble()) &&
               rssBefore.toDouble() > 0.0 && rssAfter.isDouble() &&
               std::isfinite(rssAfter.toDouble()) && rssAfter.toDouble() > 0.0 &&
               rssDelta.isDouble() && std::isfinite(rssDelta.toDouble());
    }

    [[nodiscard]] bool childSampleValid(const QJsonObject& sample,
                                        const BenchmarkWorkload& workload) {
        const auto [rows, operations, threads, readsPerThread] = workload;
        static_cast<void>(rows);
        const auto fresh = sample.value(QStringLiteral("fresh_process_open"));
        const auto repeated = sample.value(QStringLiteral("repeated_open"));
        const auto concurrent = sample.value(QStringLiteral("concurrent_keystrokes"));
        if (!fresh.isObject() || !metricObjectValid(fresh.toObject()) || !repeated.isObject() ||
            !concurrent.isObject()) {
            return false;
        }
        const auto repeatedObject = repeated.toObject();
        const auto repeatedOperations = repeatedObject.value(QStringLiteral("operations"));
        if (repeatedObject.value(QStringLiteral("burn_in_operations")).toInt(-1) !=
                kBurnInOperations ||
            !repeatedOperations.isArray() || repeatedOperations.toArray().size() != operations) {
            return false;
        }
        for (const auto operation : repeatedOperations.toArray()) {
            if (!operation.isObject() || !metricObjectValid(operation.toObject())) {
                return false;
            }
        }
        const auto expectedReads =
            static_cast<std::size_t>(threads) * static_cast<std::size_t>(readsPerThread);
        const auto concurrentObject = concurrent.toObject();
        const auto latencies = concurrentObject.value(QStringLiteral("read_wall_ms"));
        const auto latencyArray = latencies.toArray();
        if (concurrentObject.value(QStringLiteral("threads")).toInt(-1) != threads ||
            concurrentObject.value(QStringLiteral("reads_per_thread")).toInt(-1) !=
                readsPerThread ||
            concurrentObject.value(QStringLiteral("requested_reads")).toDouble(-1.0) !=
                static_cast<double>(expectedReads) ||
            concurrentObject.value(QStringLiteral("successful_reads")).toDouble(-1.0) !=
                static_cast<double>(expectedReads) ||
            concurrentObject.value(QStringLiteral("failed_reads")).toInt(-1) != 0 ||
            concurrentObject.value(QStringLiteral("canceled_reads")).toInt(-1) != 0 ||
            !metricObjectValid(concurrentObject.value(QStringLiteral("batch")).toObject()) ||
            !latencies.isArray() || latencyArray.size() != static_cast<qsizetype>(expectedReads)) {
            return false;
        }
        return std::all_of(latencyArray.begin(), latencyArray.end(), [](const QJsonValue& latency) {
            return latency.isDouble() && std::isfinite(latency.toDouble()) &&
                   latency.toDouble() >= 0.0;
        });
    }

    void appendMetric(const QJsonObject& object, std::vector<double>& wall,
                      std::vector<double>& cpu, std::vector<double>& rss) {
        wall.push_back(object.value(QStringLiteral("wall_ms")).toDouble());
        cpu.push_back(object.value(QStringLiteral("cpu_ms")).toDouble());
        rss.push_back(object.value(QStringLiteral("rss_delta_bytes")).toDouble());
    }

    [[nodiscard]] StatusLastRows statusLastRows(sqlite3* database) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(database, kStatusLastQuery.data(),
                               static_cast<int>(kStatusLastQuery.size()), &statement,
                               nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
        StatusLastRows rows;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            StatusLastRows::value_type row;
            for (int column = 0; column < 3; ++column) {
                const auto* value = sqlite3_column_text(statement, column);
                if (value == nullptr) {
                    sqlite3_finalize(statement);
                    throw std::runtime_error("status-last query returned a null fixture value");
                }
                row[static_cast<std::size_t>(column)] = reinterpret_cast<const char*>(value);
            }
            rows.push_back(std::move(row));
        }
        const int finalizeResult = sqlite3_finalize(statement);
        if (result != SQLITE_DONE || finalizeResult != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
        return rows;
    }

    [[nodiscard]] QJsonArray statusLastPlan(sqlite3* database) {
        sqlite3_stmt* statement = nullptr;
        const std::string explain = "EXPLAIN QUERY PLAN " + std::string(kStatusLastQuery);
        if (sqlite3_prepare_v2(database, explain.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
        QJsonArray plan;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            const auto* detail = sqlite3_column_text(statement, 3);
            if (detail == nullptr) {
                sqlite3_finalize(statement);
                throw std::runtime_error("status-last query plan returned a null detail");
            }
            plan.append(QString::fromUtf8(reinterpret_cast<const char*>(detail)));
        }
        const int finalizeResult = sqlite3_finalize(statement);
        if (result != SQLITE_DONE || finalizeResult != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
        return plan;
    }

    [[nodiscard]] bool planContains(const QJsonArray& plan, const std::string_view text) {
        return std::any_of(plan.cbegin(), plan.cend(), [text](const QJsonValue& entry) {
            return entry.toString().contains(
                QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size())));
        });
    }

    [[nodiscard]] StatusLastVariant runStatusLastVariant(sqlite3* database, const int samples) {
        StatusLastVariant variant;
        variant.metrics.reserve(static_cast<std::size_t>(samples));
        variant.rows.reserve(static_cast<std::size_t>(samples));
        for (int index = 0; index < samples; ++index) {
            auto [rows, metric] = measure([&] { return statusLastRows(database); });
            variant.metrics.push_back(metric);
            variant.rows.push_back(std::move(rows));
        }
        return variant;
    }

    [[nodiscard]] QJsonObject statusLastVariantJson(const StatusLastVariant& variant) {
        QJsonArray metrics;
        std::vector<double> wall;
        std::vector<double> cpu;
        std::vector<double> rss;
        wall.reserve(variant.metrics.size());
        cpu.reserve(variant.metrics.size());
        rss.reserve(variant.metrics.size());
        for (const auto& metric : variant.metrics) {
            metrics.append(metricJson(metric));
            wall.push_back(metric.wallMs);
            cpu.push_back(metric.cpuMs);
            rss.push_back(static_cast<double>(metric.rssDelta));
        }
        return {{QStringLiteral("metrics"), metrics},
                {QStringLiteral("summary"),
                 QJsonObject{{QStringLiteral("wall_ms"), summary(wall)},
                             {QStringLiteral("cpu_ms"), summary(cpu)},
                             {QStringLiteral("rss_delta_bytes"), summary(rss)}}}};
    }

    [[nodiscard]] QJsonObject runStatusLastBenchmark(const std::filesystem::path& path,
                                                     const std::size_t rows, const int samples) {
        sqlite3* database = nullptr;
        const auto encodedPath = ssa::qt::toUtf8(path);
        if (sqlite3_open_v2(encodedPath.c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) !=
            SQLITE_OK) {
            const std::string error =
                database == nullptr ? "cannot open status-last fixture" : sqlite3_errmsg(database);
            if (database != nullptr) {
                sqlite3_close(database);
            }
            throw std::runtime_error(error);
        }
        try {
            const auto baselinePlan = statusLastPlan(database);
            const auto baseline = runStatusLastVariant(database, samples);
            std::string error;
            if (!execute(database, std::string(kStatusLastIndexSql), error)) {
                throw std::runtime_error(error);
            }
            const auto indexedPlan = statusLastPlan(database);
            const auto indexed = runStatusLastVariant(database, samples);
            const auto& expectedRows = baseline.rows.front();
            const bool resultVectorsMatch =
                std::all_of(
                    baseline.rows.cbegin(), baseline.rows.cend(),
                    [&expectedRows](const StatusLastRows& rows) { return rows == expectedRows; }) &&
                std::all_of(
                    indexed.rows.cbegin(), indexed.rows.cend(),
                    [&expectedRows](const StatusLastRows& rows) { return rows == expectedRows; });
            const bool indexedUsesStatusLastIndex = planContains(indexedPlan, kStatusLastIndexName);
            const bool indexedUsesTempBtree = planContains(indexedPlan, "TEMP B-TREE");
            if (!resultVectorsMatch || !indexedUsesStatusLastIndex || indexedUsesTempBtree) {
                throw std::runtime_error(
                    "status-last benchmark result or indexed query-plan contract failed");
            }
            if (sqlite3_close(database) != SQLITE_OK) {
                throw std::runtime_error("cannot close status-last fixture");
            }
            database = nullptr;
            return {
                {QStringLiteral("scope"), QStringLiteral("sqlite_status_last_order_by_expression")},
                {QStringLiteral("sql"),
                 QString::fromLatin1(kStatusLastQuery.data(),
                                     static_cast<qsizetype>(kStatusLastQuery.size()))},
                {QStringLiteral("fixture_rows"), static_cast<qint64>(rows)},
                {QStringLiteral("os_page_cache_control"), QStringLiteral("none")},
                {QStringLiteral("sample_count"), samples},
                {QStringLiteral("plans"), QJsonObject{{QStringLiteral("baseline"), baselinePlan},
                                                      {QStringLiteral("indexed"), indexedPlan}}},
                {QStringLiteral("flags"),
                 QJsonObject{
                     {QStringLiteral("page_result_vectors_match"), resultVectorsMatch},
                     {QStringLiteral("baseline_uses_status_last_index"),
                      planContains(baselinePlan, kStatusLastIndexName)},
                     {QStringLiteral("baseline_uses_temp_btree"),
                      planContains(baselinePlan, "TEMP B-TREE")},
                     {QStringLiteral("indexed_uses_status_last_index"), indexedUsesStatusLastIndex},
                     {QStringLiteral("indexed_uses_temp_btree"), indexedUsesTempBtree}}},
                {QStringLiteral("baseline"), statusLastVariantJson(baseline)},
                {QStringLiteral("indexed"), statusLastVariantJson(indexed)}};
        } catch (...) {
            if (database != nullptr) {
                sqlite3_close_v2(database);
            }
            throw;
        }
    }

    [[nodiscard]] std::optional<QJsonObject> runChild(const std::filesystem::path& databasePath,
                                                      const BenchmarkWorkload& workload,
                                                      std::string& error) {
        QProcess worker;
        worker.setProgram(QCoreApplication::applicationFilePath());
        worker.setArguments(
            {QStringLiteral("--worker"), ssa::qt::toQString(databasePath), QStringLiteral("--rows"),
             QString::number(static_cast<qulonglong>(workload.rows)),
             QStringLiteral("--operations"), QString::number(workload.operations),
             QStringLiteral("--threads"), QString::number(workload.threads),
             QStringLiteral("--reads-per-thread"), QString::number(workload.readsPerThread)});
        worker.setProcessChannelMode(QProcess::SeparateChannels);
        worker.start();
        if (!worker.waitForStarted(10'000)) {
            error = "benchmark worker failed to start: " + worker.errorString().toStdString();
            return std::nullopt;
        }
        if (!worker.waitForFinished(kWorkerTimeoutMs)) {
            worker.kill();
            const bool terminated = worker.waitForFinished(5'000);
            error = "benchmark worker timed out after " + std::to_string(kWorkerTimeoutMs) + " ms";
            if (!terminated) {
                error += "; process did not terminate after kill";
            }
            return std::nullopt;
        }
        if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0) {
            const auto standardError = worker.readAllStandardError().trimmed().toStdString();
            error = "benchmark worker failed: exit_status=" +
                    std::to_string(static_cast<int>(worker.exitStatus())) +
                    " exit_code=" + std::to_string(worker.exitCode()) +
                    " process_error=" + worker.errorString().toStdString();
            if (!standardError.empty()) {
                error += " stderr=" + standardError;
            }
            return std::nullopt;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(worker.readAllStandardOutput(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error =
                "benchmark worker returned invalid JSON: " + parseError.errorString().toStdString();
            return std::nullopt;
        }
        if (!childSampleValid(document.object(), workload)) {
            error = "benchmark worker returned JSON that violates the sample contract";
            return std::nullopt;
        }
        return document.object();
    }

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption rowsOption(QStringLiteral("rows"), QStringLiteral("Fixture rows."),
                                        QStringLiteral("count"), QString::number(kDefaultRows));
    const QCommandLineOption samplesOption(QStringLiteral("samples"), QStringLiteral("Samples."),
                                           QStringLiteral("count"),
                                           QString::number(kDefaultSamples));
    const QCommandLineOption operationsOption(
        QStringLiteral("operations"), QStringLiteral("Repeated opens per sample."),
        QStringLiteral("count"), QString::number(kDefaultOperations));
    const QCommandLineOption threadsOption(
        QStringLiteral("threads"), QStringLiteral("Concurrent read threads."),
        QStringLiteral("count"), QString::number(kDefaultThreads));
    const QCommandLineOption readsOption(
        QStringLiteral("reads-per-thread"), QStringLiteral("Concurrent reads per thread."),
        QStringLiteral("count"), QString::number(kDefaultReadsPerThread));
    const QCommandLineOption outputOption(
        QStringLiteral("output"), QStringLiteral("JSON output path."), QStringLiteral("path"));
    const QCommandLineOption workerOption(QStringLiteral("worker"), QStringLiteral("Run worker."),
                                          QStringLiteral("database"));
    const QCommandLineOption statusLastOption(
        QStringLiteral("status-last"),
        QStringLiteral("Benchmark the status-last expression ordering query."));
    parser.addHelpOption();
    parser.addOption(rowsOption);
    parser.addOption(samplesOption);
    parser.addOption(operationsOption);
    parser.addOption(threadsOption);
    parser.addOption(readsOption);
    parser.addOption(outputOption);
    parser.addOption(workerOption);
    parser.addOption(statusLastOption);
    parser.process(application);

    bool rowsValid = false;
    bool samplesValid = false;
    bool operationsValid = false;
    bool threadsValid = false;
    bool readsValid = false;
    const auto rows = parser.value(rowsOption).toULongLong(&rowsValid);
    const int samples = parser.value(samplesOption).toInt(&samplesValid);
    const int operations = parser.value(operationsOption).toInt(&operationsValid);
    const int threads = parser.value(threadsOption).toInt(&threadsValid);
    const int readsPerThread = parser.value(readsOption).toInt(&readsValid);
    if (parser.isSet(statusLastOption) && parser.isSet(workerOption)) {
        qCritical("error: --status-last cannot be combined with --worker");
        return 2;
    }
    if (!rowsValid || rows == 0 || rows > kMaxRows || !samplesValid || samples <= 0 ||
        samples > kMaxSamples || !operationsValid || operations <= 0 ||
        operations > kMaxOperations || !threadsValid || threads <= 0 || threads > kMaxThreads ||
        !readsValid || readsPerThread <= 0 || readsPerThread > kMaxReadsPerThread ||
        static_cast<std::size_t>(threads) * static_cast<std::size_t>(readsPerThread) >
            kMaxConcurrentReads ||
        static_cast<std::size_t>(samples) * static_cast<std::size_t>(operations) >
            kMaxTotalMeasuredOperations ||
        static_cast<std::size_t>(samples) * static_cast<std::size_t>(threads) *
                static_cast<std::size_t>(readsPerThread) >
            kMaxTotalConcurrentReads) {
        qCritical("error: expected rows=1..299999999, samples=1..1000, "
                  "operations=1..10000, threads=1..64, reads-per-thread=1..10000 and at "
                  "most 1000000 concurrent reads per sample, 100000 measured operations and "
                  "100000 concurrent reads in total");
        return 2;
    }
    const BenchmarkWorkload workload{static_cast<std::size_t>(rows), operations, threads,
                                     readsPerThread};
    if (parser.isSet(workerOption)) {
        try {
            const auto result =
                runWorker(ssa::qt::toFileSystemPath(parser.value(workerOption)), workload);
            std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << '\n';
            return 0;
        } catch (const std::exception& error) {
            qCritical().noquote() << QStringLiteral("SQLITE_READ_CONNECTION worker error: %1")
                                         .arg(QString::fromStdString(error.what()));
            return 3;
        }
    }
    if (!parser.isSet(outputOption) || !parser.positionalArguments().empty()) {
        qCritical("error: --output is required");
        return 2;
    }

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        qCritical("error: cannot create benchmark directory");
        return 4;
    }
    const auto databasePath = ssa::qt::toFileSystemPath(temporary.path()) / "ssas.db";
    std::string fixtureError;
    if (!createFixture(databasePath, workload.rows, fixtureError)) {
        qCritical().noquote() << QStringLiteral("error: fixture creation failed: %1")
                                     .arg(QString::fromStdString(fixtureError));
        return 4;
    }

    QJsonObject report;
    if (parser.isSet(statusLastOption)) {
        try {
            report = runStatusLastBenchmark(databasePath, workload.rows, samples);
        } catch (const std::exception& error) {
            qCritical().noquote() << QStringLiteral("SQLITE_STATUS_LAST error: %1")
                                         .arg(QString::fromStdString(error.what()));
            return 5;
        }
    } else {
        QJsonArray samplesJson;
        std::vector<double> freshWall;
        std::vector<double> freshCpu;
        std::vector<double> freshRss;
        std::vector<double> repeatedWall;
        std::vector<double> repeatedCpu;
        std::vector<double> repeatedRss;
        std::vector<double> concurrentWall;
        std::vector<double> concurrentCpu;
        std::vector<double> concurrentRss;
        std::vector<double> concurrentLatency;
        for (int index = 0; index < samples; ++index) {
            std::string error;
            const auto sample = runChild(databasePath, workload, error);
            if (!sample) {
                qCritical().noquote() << QStringLiteral("SQLITE_READ_CONNECTION sample=%1 error=%2")
                                             .arg(index)
                                             .arg(QString::fromStdString(error));
                return 5;
            }
            samplesJson.append(*sample);
            appendMetric(sample->value(QStringLiteral("fresh_process_open")).toObject(), freshWall,
                         freshCpu, freshRss);
            const auto repeated = sample->value(QStringLiteral("repeated_open")).toObject();
            for (const auto operation : repeated.value(QStringLiteral("operations")).toArray()) {
                appendMetric(operation.toObject(), repeatedWall, repeatedCpu, repeatedRss);
            }
            const auto concurrent =
                sample->value(QStringLiteral("concurrent_keystrokes")).toObject();
            appendMetric(concurrent.value(QStringLiteral("batch")).toObject(), concurrentWall,
                         concurrentCpu, concurrentRss);
            for (const auto latency : concurrent.value(QStringLiteral("read_wall_ms")).toArray()) {
                concurrentLatency.push_back(latency.toDouble());
            }
        }
        report = {
            {QStringLiteral("scope"),
             QStringLiteral("sqlite_repository_open_busy_progress_statement_close")},
            {QStringLiteral("os_page_cache_control"), QStringLiteral("none")},
            {QStringLiteral("fresh_process_open_definition"),
             QStringLiteral("first repository read in a new worker process; not disk-cold")},
            {QStringLiteral("fixture_rows"), static_cast<qint64>(rows)},
            {QStringLiteral("sample_count"), samples},
            {QStringLiteral("limits"),
             QJsonObject{
                 {QStringLiteral("max_rows"), static_cast<qint64>(kMaxRows)},
                 {QStringLiteral("max_samples"), kMaxSamples},
                 {QStringLiteral("max_operations"), kMaxOperations},
                 {QStringLiteral("max_threads"), kMaxThreads},
                 {QStringLiteral("max_reads_per_thread"), kMaxReadsPerThread},
                 {QStringLiteral("max_concurrent_reads"), static_cast<qint64>(kMaxConcurrentReads)},
                 {QStringLiteral("max_total_measured_operations"),
                  static_cast<qint64>(kMaxTotalMeasuredOperations)},
                 {QStringLiteral("max_total_concurrent_reads"),
                  static_cast<qint64>(kMaxTotalConcurrentReads)}}},
            {QStringLiteral("samples"), samplesJson},
            {QStringLiteral("summary"),
             QJsonObject{
                 {QStringLiteral("fresh_process_open"),
                  QJsonObject{{QStringLiteral("aggregation_scope"),
                               QStringLiteral("one_first_read_per_worker_process")},
                              {QStringLiteral("wall_ms"), summary(freshWall)},
                              {QStringLiteral("cpu_ms"), summary(freshCpu)},
                              {QStringLiteral("rss_delta_bytes"), summary(freshRss)}}},
                 {QStringLiteral("repeated_open"),
                  QJsonObject{{QStringLiteral("aggregation_scope"),
                               QStringLiteral("all_measured_operations_across_worker_processes")},
                              {QStringLiteral("wall_ms"), summary(repeatedWall)},
                              {QStringLiteral("cpu_ms"), summary(repeatedCpu)},
                              {QStringLiteral("rss_delta_bytes"), summary(repeatedRss)}}},
                 {QStringLiteral("concurrent_keystrokes"),
                  QJsonObject{{QStringLiteral("batch_aggregation_scope"),
                               QStringLiteral("one_batch_per_worker_process")},
                              {QStringLiteral("read_aggregation_scope"),
                               QStringLiteral("all_read_operations_across_worker_processes")},
                              {QStringLiteral("batch_wall_ms"), summary(concurrentWall)},
                              {QStringLiteral("batch_cpu_ms"), summary(concurrentCpu)},
                              {QStringLiteral("batch_rss_delta_bytes"), summary(concurrentRss)},
                              {QStringLiteral("read_wall_ms"), summary(concurrentLatency)}}}}}};
    }
    QSaveFile output(parser.value(outputOption));
    if (!output.open(QIODevice::WriteOnly)) {
        qCritical().noquote() << QStringLiteral("error: cannot write %1").arg(output.fileName());
        return 6;
    }
    const auto payload = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (output.write(payload) != payload.size()) {
        output.cancelWriting();
        qCritical().noquote()
            << QStringLiteral("error: partial write to %1").arg(output.fileName());
        return 6;
    }
    if (!output.commit()) {
        qCritical().noquote() << QStringLiteral("error: cannot commit %1").arg(output.fileName());
        return 6;
    }
    const auto completionMessage =
        parser.isSet(statusLastOption)
            ? QStringLiteral("SQLITE_STATUS_LAST samples=%1 output=%2")
            : QStringLiteral("SQLITE_READ_CONNECTION samples=%1 output=%2");
    qInfo().noquote() << completionMessage.arg(samples).arg(output.fileName());
    return 0;
}
