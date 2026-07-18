#include "SqliteSsaImportWriterTestAccess.h"
#include "domain/ColumnCatalog.h"
#include "qt/FilesystemPath.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

    constexpr std::size_t kDefaultRows = 250'000;
    constexpr std::size_t kMaxRows = 299'999'999;
    constexpr std::int64_t kFirstSsaNumber = 700'000'000;
    constexpr std::string_view kCanonicalScenario = "canonical";
    constexpr std::string_view kLegacyScenario = "legacy";
    constexpr std::string_view kFirstPassPhase = "first-pass";
    constexpr std::string_view kIdempotentReopenPhase = "idempotent-reopen";
    constexpr std::string_view kScope =
        "sqlite_writer_incremental_session_after_fixture_preparation";
    constexpr int kWorkerStartTimeoutMs = 10'000;
    constexpr int kWorkerTimeoutMs = 60'000;

    struct Sample final {
        std::string scenario;
        std::string phase;
        double wallMs{0.0};
        double cpuMs{0.0};
        std::uint64_t rssBaselineBytes{0};
        std::uint64_t peakRssBeforeBytes{0};
        std::uint64_t currentRssBytes{0};
        std::uint64_t peakRssBytes{0};
        std::uint64_t currentRssDeltaBytes{0};
        std::uint64_t peakRssAdditionalBytes{0};
        std::size_t rowsUpdated{0};
        std::size_t rowsInserted{0};
        std::size_t rowsTotal{0};
        std::size_t nonCanonicalRows{0};
    };

    [[nodiscard]] bool isScenario(const std::string_view scenario) {
        return scenario == kCanonicalScenario || scenario == kLegacyScenario;
    }

    [[nodiscard]] bool isPhase(const std::string_view phase) {
        return phase == kFirstPassPhase || phase == kIdempotentReopenPhase;
    }

    [[nodiscard]] std::string canonicalNumber(const std::size_t offset) {
        return std::to_string(kFirstSsaNumber + static_cast<std::int64_t>(offset));
    }

    [[nodiscard]] std::string storedNumber(const std::size_t offset, const bool legacy) {
        const auto number = canonicalNumber(offset);
        return legacy ? number.substr(0, 4) + "-" + number.substr(4) + ".0" : number;
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
            if (column.key == "id") {
                sql += " INTEGER PRIMARY KEY";
            } else {
                sql += column.type == ssa::domain::ColumnType::Integer ? " INTEGER" : " TEXT";
            }
            first = false;
        }
        return sql + ")";
    }

    [[nodiscard]] bool createFixtureDatabase(const std::filesystem::path& path,
                                             const std::size_t rows, const bool legacy,
                                             std::string& error) {
        sqlite3* database = nullptr;
        const auto encodedPath = ssa::qt::toUtf8(path);
        if (sqlite3_open_v2(encodedPath.c_str(), &database,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            error = database == nullptr ? "cannot open fixture database" : sqlite3_errmsg(database);
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return false;
        }
        const auto closeDatabase = [&] { return sqlite3_close(database) == SQLITE_OK; };
        if (!execute(database, createTableSql(), error) ||
            !execute(database, "BEGIN IMMEDIATE", error)) {
            closeDatabase();
            return false;
        }

        sqlite3_stmt* insert = nullptr;
        const char* sql =
            "INSERT INTO ssa_table(numero_ssa, derivada_de, numero_ssa_relacionada_1, "
            "descricao_ssa, data_cadastro, situacao) VALUES(?, ?, ?, 'Fixture', "
            "'2026-07-17', 'APV')";
        if (sqlite3_prepare_v2(database, sql, -1, &insert, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(database);
            std::string rollbackError;
            if (!execute(database, "ROLLBACK", rollbackError) && error.empty()) {
                error = std::move(rollbackError);
            }
            closeDatabase();
            return false;
        }

        bool inserted = true;
        for (std::size_t index = 0; index < rows; ++index) {
            const auto number = storedNumber(index, legacy);
            const auto reference = index == 0 ? std::string{} : storedNumber(index - 1, legacy);
            inserted =
                sqlite3_bind_text(insert, 1, number.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
                sqlite3_bind_text(insert, 2, reference.c_str(), -1, SQLITE_TRANSIENT) ==
                    SQLITE_OK &&
                sqlite3_bind_text(insert, 3, reference.c_str(), -1, SQLITE_TRANSIENT) ==
                    SQLITE_OK &&
                sqlite3_step(insert) == SQLITE_DONE;
            if (!inserted) {
                error = sqlite3_errmsg(database);
                break;
            }
            sqlite3_reset(insert);
            sqlite3_clear_bindings(insert);
        }
        sqlite3_finalize(insert);
        if (!inserted || !execute(database, "COMMIT", error)) {
            std::string rollbackError;
            if (!execute(database, "ROLLBACK", rollbackError) && error.empty()) {
                error = std::move(rollbackError);
            }
            closeDatabase();
            return false;
        }
        if (!closeDatabase()) {
            error = "cannot close fixture database";
            return false;
        }
        return true;
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

    [[nodiscard]] std::uint64_t peakRssBytes() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS info{};
        info.cb = sizeof(info);
        return GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)) == 0
                   ? 0
                   : static_cast<std::uint64_t>(info.PeakWorkingSetSize);
#else
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0;
        }
#ifdef __APPLE__
        return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
#endif
    }

    [[nodiscard]] std::optional<std::size_t> scalarCount(sqlite3* database,
                                                         const std::string& sql) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }
        const bool hasRow = sqlite3_step(statement) == SQLITE_ROW;
        const auto value =
            hasRow ? static_cast<std::size_t>(sqlite3_column_int64(statement, 0)) : 0;
        sqlite3_finalize(statement);
        return hasRow ? std::optional{value} : std::nullopt;
    }

    [[nodiscard]] std::string canonicalityViolationPredicate() {
        return "(numero_ssa IS NULL OR TYPEOF(numero_ssa) <> 'text' OR "
               "INSTR(numero_ssa, CHAR(0)) <> 0 OR numero_ssa NOT GLOB "
               "'[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]') OR "
               "(derivada_de IS NOT NULL AND (TYPEOF(derivada_de) <> 'text' OR "
               "INSTR(derivada_de, CHAR(0)) <> 0 OR "
               "(derivada_de <> '' AND derivada_de NOT GLOB "
               "'[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'))) OR "
               "(numero_ssa_relacionada_1 IS NOT NULL AND "
               "(TYPEOF(numero_ssa_relacionada_1) <> 'text' OR "
               "INSTR(numero_ssa_relacionada_1, CHAR(0)) <> 0 OR "
               "(numero_ssa_relacionada_1 <> '' AND numero_ssa_relacionada_1 NOT GLOB "
               "'[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'))) OR "
               "(numero_ssa_relacionada_2 IS NOT NULL AND "
               "(TYPEOF(numero_ssa_relacionada_2) <> 'text' OR "
               "INSTR(numero_ssa_relacionada_2, CHAR(0)) <> 0 OR "
               "(numero_ssa_relacionada_2 <> '' AND numero_ssa_relacionada_2 NOT GLOB "
               "'[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'))) OR "
               "(numero_ssa_relacionada_3 IS NOT NULL AND "
               "(TYPEOF(numero_ssa_relacionada_3) <> 'text' OR "
               "INSTR(numero_ssa_relacionada_3, CHAR(0)) <> 0 OR "
               "(numero_ssa_relacionada_3 <> '' AND numero_ssa_relacionada_3 NOT GLOB "
               "'[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]')))";
    }

    [[nodiscard]] std::optional<bool> canonicalIndexContract(sqlite3* database) {
        const auto legacyDirtyIndexes = scalarCount(
            database, "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name IN ("
                      "'idx_ssa_table_import_dirty_numero_ssa', "
                      "'idx_ssa_table_import_dirty_derivada_de', "
                      "'idx_ssa_table_import_dirty_numero_ssa_relacionada_1', "
                      "'idx_ssa_table_import_dirty_numero_ssa_relacionada_2', "
                      "'idx_ssa_table_import_dirty_numero_ssa_relacionada_3')");
        if (!legacyDirtyIndexes) {
            return std::nullopt;
        }
        const auto normalSsaIndex =
            scalarCount(database, "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
                                  "name='idx_ssa_table_numero_ssa'");
        if (!normalSsaIndex) {
            return std::nullopt;
        }
        const auto canonicalIndexes = scalarCount(
            database,
            "SELECT COUNT(*) FROM pragma_index_list('ssa_table') WHERE "
            "(name='ux_ssa_table_numero_ssa' AND \"unique\"=1 AND partial=0) OR "
            "(name='idx_ssa_table_import_dirty_canonical' AND \"unique\"=0 AND partial=1)");
        return canonicalIndexes && *legacyDirtyIndexes == 0 && *normalSsaIndex == 0 &&
               *canonicalIndexes == 2;
    }

    [[nodiscard]] bool databaseContract(const std::filesystem::path& path, const std::size_t rows,
                                        Sample& sample, std::string& error) {
        sqlite3* database = nullptr;
        const auto encodedPath = ssa::qt::toUtf8(path);
        if (sqlite3_open_v2(encodedPath.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) !=
            SQLITE_OK) {
            error =
                database == nullptr ? "cannot reopen benchmark database" : sqlite3_errmsg(database);
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return false;
        }
        const auto closeDatabase = [&] { return sqlite3_close(database) == SQLITE_OK; };
        const auto total = scalarCount(database, "SELECT COUNT(*) FROM ssa_table");
        const auto incoming =
            scalarCount(database, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='" +
                                      canonicalNumber(rows) + "'");
        const auto nonCanonical = scalarCount(database, "SELECT COUNT(*) FROM ssa_table WHERE " +
                                                            canonicalityViolationPredicate());
        const auto indexes = canonicalIndexContract(database);
        sqlite3_stmt* integrity = nullptr;
        const bool prepared = sqlite3_prepare_v2(database, "PRAGMA integrity_check", -1, &integrity,
                                                 nullptr) == SQLITE_OK;
        const auto* integrityText = prepared && sqlite3_step(integrity) == SQLITE_ROW
                                        ? sqlite3_column_text(integrity, 0)
                                        : nullptr;
        const bool valid = integrityText != nullptr &&
                           std::string_view(reinterpret_cast<const char*>(integrityText)) == "ok";
        if (integrity != nullptr) {
            sqlite3_finalize(integrity);
        }
        const bool closed = closeDatabase();
        if (!total || !incoming || !nonCanonical || !indexes || !valid || !closed) {
            error = "benchmark database contract unavailable";
            return false;
        }
        sample.rowsTotal = *total;
        sample.nonCanonicalRows = *nonCanonical;
        if (*total == rows + 1 && *incoming == 1 && *nonCanonical == 0 && *indexes) {
            return true;
        }
        error = "benchmark database contract failed total=" + std::to_string(*total) +
                " incoming=" + std::to_string(*incoming) +
                " noncanonical=" + std::to_string(*nonCanonical) +
                " indexes=" + std::to_string(*indexes);
        return false;
    }

    [[nodiscard]] bool prewarmDatabaseContract(const std::filesystem::path& path,
                                               const std::size_t rows, std::string& error) {
        sqlite3* database = nullptr;
        const auto encodedPath = ssa::qt::toUtf8(path);
        if (sqlite3_open_v2(encodedPath.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) !=
            SQLITE_OK) {
            error = database == nullptr ? "cannot reopen prewarmed benchmark database"
                                        : sqlite3_errmsg(database);
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return false;
        }
        const auto closeDatabase = [&] { return sqlite3_close(database) == SQLITE_OK; };
        const auto total = scalarCount(database, "SELECT COUNT(*) FROM ssa_table");
        const auto nonCanonical = scalarCount(database, "SELECT COUNT(*) FROM ssa_table WHERE " +
                                                            canonicalityViolationPredicate());
        const auto indexes = canonicalIndexContract(database);
        const bool closed = closeDatabase();
        if (!total || !nonCanonical || !indexes || !closed) {
            error = "prewarmed benchmark database contract unavailable";
            return false;
        }
        if (*total == rows && *nonCanonical == 0 && *indexes) {
            return true;
        }
        error = "prewarmed benchmark database contract failed total=" + std::to_string(*total) +
                " noncanonical=" + std::to_string(*nonCanonical) +
                " indexes=" + std::to_string(*indexes);
        return false;
    }

    [[nodiscard]] QJsonObject sampleJson(const Sample& sample) {
        return {
            {QStringLiteral("scope"), QString::fromUtf8(kScope.data(), kScope.size())},
            {QStringLiteral("scenario"), QString::fromStdString(sample.scenario)},
            {QStringLiteral("phase"), QString::fromStdString(sample.phase)},
            {QStringLiteral("wall_ms"), sample.wallMs},
            {QStringLiteral("cpu_ms"), sample.cpuMs},
            {QStringLiteral("rss_baseline_bytes"), static_cast<qint64>(sample.rssBaselineBytes)},
            {QStringLiteral("peak_rss_before_bytes"),
             static_cast<qint64>(sample.peakRssBeforeBytes)},
            {QStringLiteral("current_rss_bytes"), static_cast<qint64>(sample.currentRssBytes)},
            {QStringLiteral("peak_rss_bytes"), static_cast<qint64>(sample.peakRssBytes)},
            {QStringLiteral("current_rss_delta_bytes"),
             static_cast<qint64>(sample.currentRssDeltaBytes)},
            {QStringLiteral("peak_rss_additional_bytes"),
             static_cast<qint64>(sample.peakRssAdditionalBytes)},
            {QStringLiteral("rows_updated"), static_cast<qint64>(sample.rowsUpdated)},
            {QStringLiteral("rows_inserted"), static_cast<qint64>(sample.rowsInserted)},
            {QStringLiteral("rows_total"), static_cast<qint64>(sample.rowsTotal)},
            {QStringLiteral("noncanonical_identity_or_reference_rows"),
             static_cast<qint64>(sample.nonCanonicalRows)}};
    }

    [[nodiscard]] bool prewarmSample(const std::filesystem::path& databasePath,
                                     const std::size_t rows, std::string& error) {
        try {
            const auto columns = ssa::domain::ColumnCatalog::all();
            const ssa::infra::sqlite::SqliteSsaImportWriter writer(
                ssa::infra::sqlite::SqliteSsaImportWriterTestAccess::access(), databasePath,
                {columns.begin(), columns.end()});
            auto session = writer.startSession(false);
            const auto summary = session.finish();
            if (summary.rowsInserted != 0 || summary.rowsWritten != summary.rowsUpdated ||
                !prewarmDatabaseContract(databasePath, rows, error)) {
                if (error.empty()) {
                    error = "prewarmed writer contract failed";
                }
                return false;
            }
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        }
    }

    [[nodiscard]] bool prepareSample(const std::filesystem::path& root,
                                     const std::string_view scenario, const std::string_view phase,
                                     const std::size_t rows, std::string& error) {
        std::error_code directoryError;
        const auto databasePath = root / "data" / "ssas.db";
        std::filesystem::create_directories(databasePath.parent_path(), directoryError);
        return !directoryError &&
               createFixtureDatabase(databasePath, rows, scenario == kLegacyScenario, error) &&
               (phase != kIdempotentReopenPhase || prewarmSample(databasePath, rows, error));
    }

    [[nodiscard]] std::optional<Sample> runWorker(const std::string_view scenario,
                                                  const std::string_view phase,
                                                  const std::filesystem::path& databasePath,
                                                  const std::size_t rows, std::string& error) {
        if (!isScenario(scenario) || !isPhase(phase)) {
            error = "invalid worker scenario or phase";
            return std::nullopt;
        }
        const auto columns = ssa::domain::ColumnCatalog::all();
        const ssa::infra::sqlite::SqliteSsaImportWriter writer(
            ssa::infra::sqlite::SqliteSsaImportWriterTestAccess::access(), databasePath,
            {columns.begin(), columns.end()});
        ssa::infra::importing::ResolvedSsaImportRows incoming;
        incoming.rows.push_back({{"numero_ssa", canonicalNumber(rows)},
                                 {"descricao_ssa", "Incoming"},
                                 {"data_cadastro", "2026-07-17"},
                                 {"situacao", "APV"}});
        const auto rssBefore = currentRssBytes();
        const auto peakBefore = peakRssBytes();
        const auto cpuBefore = processCpuNanoseconds();
        const auto started = std::chrono::steady_clock::now();
        auto session = writer.startSession(false);
        const auto batch = session.write(incoming, 1, 0);
        const auto summary = session.finish();
        const auto finished = std::chrono::steady_clock::now();
        const auto cpuAfter = processCpuNanoseconds();
        if (!cpuBefore || !cpuAfter || *cpuAfter < *cpuBefore || rssBefore == 0 ||
            peakBefore == 0) {
            error = "normalization writer metrics unavailable";
            return std::nullopt;
        }

        Sample sample;
        sample.scenario = std::string{scenario};
        sample.phase = std::string{phase};
        sample.wallMs = std::chrono::duration<double, std::milli>(finished - started).count();
        sample.cpuMs = static_cast<double>(*cpuAfter - *cpuBefore) / 1'000'000.0;
        sample.rssBaselineBytes = rssBefore;
        sample.peakRssBeforeBytes = peakBefore;
        sample.currentRssBytes = currentRssBytes();
        sample.peakRssBytes = peakRssBytes();
        sample.currentRssDeltaBytes =
            sample.currentRssBytes >= rssBefore ? sample.currentRssBytes - rssBefore : 0;
        sample.peakRssAdditionalBytes =
            sample.peakRssBytes >= peakBefore ? sample.peakRssBytes - peakBefore : 0;
        sample.rowsUpdated = summary.rowsUpdated;
        sample.rowsInserted = summary.rowsInserted;
        const auto expectedUpdated =
            phase == kFirstPassPhase && scenario == kLegacyScenario ? rows : 0;
        if (sample.currentRssBytes == 0 || sample.peakRssBytes == 0 ||
            sample.rowsUpdated != expectedUpdated || sample.rowsInserted != 1 ||
            batch.rowsInserted != 1 || !databaseContract(databasePath, rows, sample, error)) {
            if (error.empty()) {
                error = "normalization worker contract failed";
            }
            return std::nullopt;
        }
        return sample;
    }

    [[nodiscard]] std::optional<Sample> runChild(const std::string_view scenario,
                                                 const std::string_view phase,
                                                 const std::filesystem::path& root,
                                                 const std::size_t rows, std::string& error) {
        QProcess worker;
        worker.setProgram(QCoreApplication::applicationFilePath());
        worker.setArguments(
            {QStringLiteral("--worker"),
             QString::fromUtf8(scenario.data(), static_cast<qsizetype>(scenario.size())),
             QString::fromUtf8(phase.data(), static_cast<qsizetype>(phase.size())),
             ssa::qt::toQString(root / "data" / "ssas.db"),
             QString::number(static_cast<qulonglong>(rows))});
        worker.setProcessChannelMode(QProcess::SeparateChannels);
        worker.start();
        if (!worker.waitForStarted(kWorkerStartTimeoutMs)) {
            error = "normalization worker start failed: " + worker.errorString().toStdString();
            return std::nullopt;
        }
        if (!worker.waitForFinished(kWorkerTimeoutMs)) {
            worker.kill();
            worker.waitForFinished();
            error = "normalization worker timeout after " + std::to_string(kWorkerTimeoutMs) +
                    "ms: " + worker.readAllStandardError().trimmed().toStdString();
            return std::nullopt;
        }
        if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0) {
            error = worker.readAllStandardError().trimmed().toStdString();
            if (error.empty()) {
                error =
                    "normalization worker exited with code " + std::to_string(worker.exitCode());
            }
            return std::nullopt;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(worker.readAllStandardOutput(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = "normalization worker returned invalid JSON";
            return std::nullopt;
        }
        const auto object = document.object();
        Sample sample;
        sample.scenario = object.value(QStringLiteral("scenario")).toString().toStdString();
        sample.phase = object.value(QStringLiteral("phase")).toString().toStdString();
        sample.wallMs = object.value(QStringLiteral("wall_ms")).toDouble();
        sample.cpuMs = object.value(QStringLiteral("cpu_ms")).toDouble();
        sample.rssBaselineBytes = static_cast<std::uint64_t>(
            object.value(QStringLiteral("rss_baseline_bytes")).toInteger());
        sample.peakRssBeforeBytes = static_cast<std::uint64_t>(
            object.value(QStringLiteral("peak_rss_before_bytes")).toInteger());
        sample.currentRssBytes = static_cast<std::uint64_t>(
            object.value(QStringLiteral("current_rss_bytes")).toInteger());
        sample.peakRssBytes =
            static_cast<std::uint64_t>(object.value(QStringLiteral("peak_rss_bytes")).toInteger());
        sample.currentRssDeltaBytes = static_cast<std::uint64_t>(
            object.value(QStringLiteral("current_rss_delta_bytes")).toInteger());
        sample.peakRssAdditionalBytes = static_cast<std::uint64_t>(
            object.value(QStringLiteral("peak_rss_additional_bytes")).toInteger());
        sample.rowsUpdated =
            static_cast<std::size_t>(object.value(QStringLiteral("rows_updated")).toInteger());
        sample.rowsInserted =
            static_cast<std::size_t>(object.value(QStringLiteral("rows_inserted")).toInteger());
        sample.rowsTotal =
            static_cast<std::size_t>(object.value(QStringLiteral("rows_total")).toInteger());
        sample.nonCanonicalRows = static_cast<std::size_t>(
            object.value(QStringLiteral("noncanonical_identity_or_reference_rows")).toInteger());
        if (sample.scenario != scenario || sample.phase != phase || sample.wallMs < 0.0 ||
            sample.cpuMs < 0.0 || sample.rowsTotal != rows + 1 || sample.rowsInserted != 1 ||
            sample.nonCanonicalRows != 0) {
            error = "normalization worker returned invalid metrics";
            return std::nullopt;
        }
        return sample;
    }

    [[nodiscard]] double percentile(std::vector<double> values, const double fraction) {
        std::sort(values.begin(), values.end());
        const auto index =
            static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * fraction)) - 1;
        return values[index];
    }

    template <typename Metric>
    [[nodiscard]] QJsonObject summarize(const std::vector<Sample>& samples, Metric metric) {
        std::vector<double> values;
        values.reserve(samples.size());
        std::transform(
            samples.begin(), samples.end(), std::back_inserter(values),
            [metric](const Sample& sample) { return static_cast<double>(sample.*metric); });
        return {{QStringLiteral("p50"), percentile(values, 0.50)},
                {QStringLiteral("p95"), percentile(std::move(values), 0.95)}};
    }

    [[nodiscard]] QJsonObject summarizeScenario(const std::vector<Sample>& samples) {
        return {{QStringLiteral("wall_ms"), summarize(samples, &Sample::wallMs)},
                {QStringLiteral("cpu_ms"), summarize(samples, &Sample::cpuMs)},
                {QStringLiteral("current_rss_delta_bytes"),
                 summarize(samples, &Sample::currentRssDeltaBytes)},
                {QStringLiteral("peak_rss_additional_bytes"),
                 summarize(samples, &Sample::peakRssAdditionalBytes)}};
    }

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption samplesOption(QStringLiteral("samples"),
                                           QStringLiteral("Number of samples per scenario."),
                                           QStringLiteral("count"), QStringLiteral("1"));
    const QCommandLineOption rowsOption(QStringLiteral("rows"),
                                        QStringLiteral("Fixture rows per sample."),
                                        QStringLiteral("count"), QString::number(kDefaultRows));
    const QCommandLineOption phaseOption(
        QStringLiteral("phase"),
        QStringLiteral("Benchmark phase: first-pass or idempotent-reopen."),
        QStringLiteral("phase"), QString::fromUtf8(kFirstPassPhase.data(), kFirstPassPhase.size()));
    const QCommandLineOption workerOption(
        QStringLiteral("worker"), QStringLiteral("Run one prepared worker scenario and phase."),
        QStringLiteral("scenario"));
    parser.addHelpOption();
    parser.addOption(samplesOption);
    parser.addOption(rowsOption);
    parser.addOption(phaseOption);
    parser.addOption(workerOption);
    parser.process(application);

    const auto positional = parser.positionalArguments();
    if (parser.isSet(workerOption)) {
        bool rowsValid = false;
        const auto scenario = parser.value(workerOption).toStdString();
        const auto phase = positional.size() == 3 ? positional.at(0).toStdString() : std::string{};
        const auto rows = positional.size() == 3 ? positional.at(2).toULongLong(&rowsValid) : 0;
        std::string error;
        const auto sample =
            isScenario(scenario) && isPhase(phase) && rowsValid && rows > 0 && rows <= kMaxRows
                ? runWorker(scenario, phase, ssa::qt::toFileSystemPath(positional.at(1)),
                            static_cast<std::size_t>(rows), error)
                : std::nullopt;
        if (!sample) {
            qCritical().noquote() << QStringLiteral("SSA_IMPORT_NORMALIZATION worker_error=%1")
                                         .arg(QString::fromStdString(
                                             error.empty() ? "invalid worker arguments" : error));
            return 2;
        }
        std::cout << QJsonDocument(sampleJson(*sample)).toJson(QJsonDocument::Compact).toStdString()
                  << '\n';
        return 0;
    }

    bool samplesValid = false;
    bool rowsValid = false;
    const int requestedSamples = parser.value(samplesOption).toInt(&samplesValid);
    const auto requestedRows = parser.value(rowsOption).toULongLong(&rowsValid);
    const auto phase = parser.value(phaseOption).toStdString();
    if (!samplesValid || requestedSamples <= 0 || !rowsValid || requestedRows == 0 ||
        requestedRows > kMaxRows || !isPhase(phase) || !positional.empty()) {
        qCritical("error: --samples and --rows must be positive integers and --phase "
                  "first-pass or idempotent-reopen");
        return 2;
    }
    const auto rows = static_cast<std::size_t>(requestedRows);

    std::vector<Sample> canonical;
    std::vector<Sample> legacy;
    canonical.reserve(static_cast<std::size_t>(requestedSamples));
    legacy.reserve(static_cast<std::size_t>(requestedSamples));
    for (const auto scenario : {kCanonicalScenario, kLegacyScenario}) {
        auto& samples = scenario == kCanonicalScenario ? canonical : legacy;
        for (int index = 0; index < requestedSamples; ++index) {
            QTemporaryDir temporary;
            std::string error;
            const auto root = ssa::qt::toFileSystemPath(temporary.path());
            if (!temporary.isValid() || !prepareSample(root, scenario, phase, rows, error)) {
                qCritical().noquote()
                    << QStringLiteral("SSA_IMPORT_NORMALIZATION fixture scenario=%1 phase=%2 "
                                      "sample=%3 error=%4")
                           .arg(QString::fromUtf8(scenario.data(),
                                                  static_cast<qsizetype>(scenario.size())))
                           .arg(QString::fromStdString(phase))
                           .arg(index)
                           .arg(QString::fromStdString(error.empty() ? "fixture preparation failed"
                                                                     : error));
                return 3;
            }
            const auto sample = runChild(scenario, phase, root, rows, error);
            if (!sample) {
                qCritical().noquote()
                    << QStringLiteral("SSA_IMPORT_NORMALIZATION scenario=%1 phase=%2 sample=%3 "
                                      "error=%4")
                           .arg(QString::fromUtf8(scenario.data(),
                                                  static_cast<qsizetype>(scenario.size())))
                           .arg(QString::fromStdString(phase))
                           .arg(index)
                           .arg(QString::fromStdString(error));
                return 4;
            }
            samples.push_back(*sample);
            auto sampleObject = sampleJson(*sample);
            sampleObject.insert(QStringLiteral("sample_index"), index);
            sampleObject.insert(QStringLiteral("fixture_rows"), static_cast<qint64>(rows));
            std::cout << "SSA_IMPORT_NORMALIZATION_SAMPLE "
                      << QJsonDocument(sampleObject).toJson(QJsonDocument::Compact).toStdString()
                      << '\n';
        }
    }
    const QJsonObject summary{
        {QStringLiteral("scope"), QString::fromUtf8(kScope.data(), kScope.size())},
        {QStringLiteral("phase"), QString::fromStdString(phase)},
        {QStringLiteral("fixture_rows"), static_cast<qint64>(rows)},
        {QStringLiteral("samples_per_scenario"), requestedSamples},
        {QStringLiteral("worker_timeout_ms"), kWorkerTimeoutMs},
        {QStringLiteral("aggregate_worker_timeout_ms"),
         static_cast<qint64>(2LL * requestedSamples * kWorkerTimeoutMs)},
        {QStringLiteral("canonical"), summarizeScenario(canonical)},
        {QStringLiteral("legacy"), summarizeScenario(legacy)}};
    std::cout << "SSA_IMPORT_NORMALIZATION_SUMMARY "
              << QJsonDocument(summary).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
}
