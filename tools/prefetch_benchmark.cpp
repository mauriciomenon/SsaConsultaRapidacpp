#include "ports/ISsaBrowsePort.h"
#include "presentation/PageQueryCoordinator.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

    constexpr std::size_t kPageSize = 10;
    constexpr std::size_t kTotalRows = 30;
    constexpr int kOperationTimeoutMs = 5'000;

    class BenchmarkBrowsePort final : public ssa::ports::ISsaBrowsePort {
      public:
        [[nodiscard]] ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                                      std::stop_token = {}) const override {
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            const ssa::domain::SsaRecord row{
                std::map<std::string, std::string>{{"numero_ssa", "202600001"}}};
            return {{row}, kTotalRows, request.pageIndex, request.pageSize};
        }

        [[nodiscard]] std::size_t count(const ssa::domain::SsaPageRequest&,
                                        std::stop_token = {}) const override {
            const std::scoped_lock lock(mutex_);
            ++countCalls_;
            return kTotalRows;
        }

        [[nodiscard]] std::optional<ssa::domain::SsaRecord>
        details(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest&,
                       std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        [[nodiscard]] ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                                        ssa::ports::SsaRecordConsumer,
                                                        std::stop_token = {}) const override {
            return {0, {}};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

        [[nodiscard]] std::size_t countCalls() const {
            const std::scoped_lock lock(mutex_);
            return countCalls_;
        }

      private:
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
        mutable std::size_t countCalls_{0};
    };

    struct Sample final {
        double initializationWallMs{0.0};
        double foregroundWallMs{0.0};
        double foregroundCpuMs{0.0};
        double idleWallMs{0.0};
        double idleCpuMs{0.0};
        std::uint64_t rssBytes{0};
        std::size_t countCalls{0};
        int initialTerminalCount{0};
        std::vector<std::size_t> requestPages;
        bool cacheHitsVerified{false};
    };

    [[nodiscard]] double milliseconds(const qint64 nanoseconds) {
        return static_cast<double>(nanoseconds) / 1'000'000.0;
    }

#ifdef _WIN32
    [[nodiscard]] std::uint64_t fileTimeNanoseconds(const FILETIME value) {
        ULARGE_INTEGER raw{};
        raw.LowPart = value.dwLowDateTime;
        raw.HighPart = value.dwHighDateTime;
        return raw.QuadPart * 100ULL;
    }
#endif

    [[nodiscard]] std::optional<std::uint64_t> processCpuNanoseconds() {
#ifdef _WIN32
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0) {
            return std::nullopt;
        }
        return fileTimeNanoseconds(kernel) + fileTimeNanoseconds(user);
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

    [[nodiscard]] bool
    exactPrefetchRequests(const std::vector<ssa::domain::SsaPageRequest>& requests) {
        if (requests.size() != 3) {
            return false;
        }
        std::vector<std::size_t> pages;
        pages.reserve(requests.size());
        for (const auto& request : requests) {
            pages.push_back(request.pageIndex);
        }
        std::sort(pages.begin(), pages.end());
        return pages == std::vector<std::size_t>{0, 1, 2};
    }

    [[nodiscard]] std::optional<Sample> runSample(QString& error) {
        QElapsedTimer elapsed;
        elapsed.start();
        auto port = std::make_shared<BenchmarkBrowsePort>();
        ssa::presentation::PageQueryCoordinator coordinator(port);
        Sample sample;
        sample.initializationWallMs = milliseconds(elapsed.nsecsElapsed());

        ssa::domain::SsaPageRequest request;
        request.pageSize = kPageSize;
        request.visibleColumns = {"numero_ssa"};

        QEventLoop foregroundLoop;
        QTimer foregroundTimeout;
        foregroundTimeout.setSingleShot(true);
        int succeededCount = 0;
        qint64 foregroundFinishedAt = 0;
        std::optional<std::uint64_t> foregroundCpuEnd;
        QObject::connect(&foregroundTimeout, &QTimer::timeout, [&] {
            error = QStringLiteral("foreground operation timed out");
            foregroundLoop.quit();
        });
        QObject::connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed,
                         [&](const QString& message) {
                             error = QStringLiteral("foreground failed: %1").arg(message);
                             foregroundLoop.quit();
                         });
        QObject::connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, [&] {
            error = QStringLiteral("foreground was canceled");
            foregroundLoop.quit();
        });
        QObject::connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded,
                         [&](const ssa::presentation::PageQueryResult&,
                             const ssa::domain::SsaPageRequest& completedRequest) {
                             ++succeededCount;
                             if (completedRequest.pageIndex != 0) {
                                 return;
                             }
                             ++sample.initialTerminalCount;
                             foregroundFinishedAt = elapsed.nsecsElapsed();
                             foregroundCpuEnd = processCpuNanoseconds();
                             foregroundLoop.quit();
                         });

        const auto foregroundStartedAt = elapsed.nsecsElapsed();
        const auto foregroundCpuStart = processCpuNanoseconds();
        foregroundTimeout.start(kOperationTimeoutMs);
        coordinator.run(request);
        foregroundLoop.exec();
        foregroundTimeout.stop();
        if (!error.isEmpty() || sample.initialTerminalCount != 1 || !foregroundCpuStart ||
            !foregroundCpuEnd || *foregroundCpuEnd < *foregroundCpuStart) {
            if (error.isEmpty()) {
                error = QStringLiteral("foreground terminal or CPU contract failed");
            }
            return std::nullopt;
        }
        sample.foregroundWallMs = milliseconds(foregroundFinishedAt - foregroundStartedAt);
        sample.foregroundCpuMs =
            milliseconds(static_cast<qint64>(*foregroundCpuEnd - *foregroundCpuStart));

        const auto idleStartedAt = foregroundFinishedAt;
        const auto idleCpuStart = *foregroundCpuEnd;
        qint64 idleFinishedAt = idleStartedAt;
        std::optional<std::uint64_t> idleCpuEnd;
        QEventLoop idleLoop;
        QTimer idleTimeout;
        idleTimeout.setSingleShot(true);
        const auto finishIdle = [&] {
            idleFinishedAt = elapsed.nsecsElapsed();
            idleCpuEnd = processCpuNanoseconds();
            idleLoop.quit();
        };
        const auto activeConnection = QObject::connect(
            &coordinator, &ssa::presentation::PageQueryCoordinator::activeOperationsChanged, [&] {
                if (!coordinator.hasActiveOperations()) {
                    finishIdle();
                }
            });
        QObject::connect(&idleTimeout, &QTimer::timeout, [&] {
            error = QStringLiteral("prefetch idle operation timed out");
            idleLoop.quit();
        });
        if (coordinator.hasActiveOperations()) {
            idleTimeout.start(kOperationTimeoutMs);
            idleLoop.exec();
            idleTimeout.stop();
        } else {
            finishIdle();
        }
        QObject::disconnect(activeConnection);
        if (!error.isEmpty() || coordinator.hasActiveOperations() || !idleCpuEnd ||
            *idleCpuEnd < idleCpuStart) {
            if (error.isEmpty()) {
                error = QStringLiteral("prefetch idle or CPU contract failed");
            }
            return std::nullopt;
        }
        sample.idleWallMs = milliseconds(idleFinishedAt - idleStartedAt);
        sample.idleCpuMs = milliseconds(static_cast<qint64>(*idleCpuEnd - idleCpuStart));

        const auto prefetchedRequests = port->requests();
        if (!exactPrefetchRequests(prefetchedRequests) || port->countCalls() != 1) {
            error = QStringLiteral("prefetch request contract failed");
            return std::nullopt;
        }
        sample.requestPages.reserve(prefetchedRequests.size());
        for (const auto& prefetched : prefetchedRequests) {
            sample.requestPages.push_back(prefetched.pageIndex);
        }
        std::sort(sample.requestPages.begin(), sample.requestPages.end());

        const auto succeededBeforeCacheHits = succeededCount;
        for (const std::size_t pageIndex : {std::size_t{1}, std::size_t{2}}) {
            auto cachedRequest = request;
            cachedRequest.pageIndex = pageIndex;
            coordinator.run(std::move(cachedRequest));
        }
        sample.cacheHitsVerified = succeededCount == succeededBeforeCacheHits + 2 &&
                                   exactPrefetchRequests(port->requests());
        sample.countCalls = port->countCalls();
        sample.rssBytes = currentRssBytes();
        if (!sample.cacheHitsVerified || sample.rssBytes == 0) {
            error = QStringLiteral("cache or RSS contract failed");
            return std::nullopt;
        }
        return sample;
    }

    [[nodiscard]] double median(std::vector<double> values) {
        std::sort(values.begin(), values.end());
        const auto middle = values.size() / 2;
        return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) / 2.0
                                      : values[middle];
    }

    [[nodiscard]] double nearestRank(std::vector<double> values, const double fraction) {
        std::sort(values.begin(), values.end());
        const auto index =
            static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size()))) - 1;
        return values[index];
    }

    [[nodiscard]] QJsonObject summarize(const std::vector<Sample>& samples,
                                        const double Sample::* member) {
        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto& sample : samples) {
            values.push_back(sample.*member);
        }
        return {{QStringLiteral("median"), median(values)},
                {QStringLiteral("p95"), nearestRank(std::move(values), 0.95)}};
    }

    [[nodiscard]] QJsonObject summarizeRss(const std::vector<Sample>& samples) {
        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto& sample : samples) {
            values.push_back(static_cast<double>(sample.rssBytes));
        }
        return {{QStringLiteral("median"), median(values)},
                {QStringLiteral("p95"), nearestRank(std::move(values), 0.95)}};
    }

    [[nodiscard]] QJsonObject sampleJson(const Sample& sample) {
        QJsonArray requestPages;
        for (const auto page : sample.requestPages) {
            requestPages.append(static_cast<qint64>(page));
        }
        return {
            {QStringLiteral("initialization_wall_ms"), sample.initializationWallMs},
            {QStringLiteral("foreground_wall_ms"), sample.foregroundWallMs},
            {QStringLiteral("foreground_cpu_ms"), sample.foregroundCpuMs},
            {QStringLiteral("idle_wall_ms"), sample.idleWallMs},
            {QStringLiteral("idle_cpu_ms"), sample.idleCpuMs},
            {QStringLiteral("rss_bytes"), static_cast<qint64>(sample.rssBytes)},
            {QStringLiteral("count_calls"), static_cast<qint64>(sample.countCalls)},
            {QStringLiteral("initial_terminal_count"), sample.initialTerminalCount},
            {QStringLiteral("request_pages"), requestPages},
            {QStringLiteral("cache_hit_pages"), QJsonArray{1, 2}},
            {QStringLiteral("cache_hits_verified"), sample.cacheHitsVerified},
        };
    }

} // namespace

int main(int argc, char* argv[]) {
    QElapsedTimer mainSetup;
    mainSetup.start();
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption samplesOption(QStringLiteral("samples"),
                                           QStringLiteral("Number of samples."),
                                           QStringLiteral("count"), QStringLiteral("1"));
    const QCommandLineOption outputOption(
        QStringLiteral("output"), QStringLiteral("JSON output path."), QStringLiteral("path"));
    parser.addHelpOption();
    parser.addOption(samplesOption);
    parser.addOption(outputOption);
    parser.process(application);

    bool samplesValid = false;
    const int requestedSamples = parser.value(samplesOption).toInt(&samplesValid);
    if (!samplesValid || requestedSamples <= 0 || !parser.isSet(outputOption)) {
        qCritical("error: --samples must be positive and --output is required");
        return 2;
    }
    const double mainSetupWallMs = milliseconds(mainSetup.nsecsElapsed());

    std::vector<Sample> samples;
    samples.reserve(static_cast<std::size_t>(requestedSamples));
    for (int index = 0; index < requestedSamples; ++index) {
        QString error;
        auto sample = runSample(error);
        if (!sample.has_value()) {
            qCritical().noquote()
                << QStringLiteral("PREFETCH_BENCHMARK sample=%1 error=%2").arg(index).arg(error);
            return 3;
        }
        samples.push_back(std::move(*sample));
    }

    QJsonArray sampleArray;
    for (const auto& sample : samples) {
        sampleArray.append(sampleJson(sample));
    }
    const QJsonObject summary{
        {QStringLiteral("initialization_wall_ms"),
         summarize(samples, &Sample::initializationWallMs)},
        {QStringLiteral("foreground_wall_ms"), summarize(samples, &Sample::foregroundWallMs)},
        {QStringLiteral("foreground_cpu_ms"), summarize(samples, &Sample::foregroundCpuMs)},
        {QStringLiteral("idle_wall_ms"), summarize(samples, &Sample::idleWallMs)},
        {QStringLiteral("idle_cpu_ms"), summarize(samples, &Sample::idleCpuMs)},
        {QStringLiteral("rss_bytes"), summarizeRss(samples)},
    };
    const QJsonObject report{
        {QStringLiteral("main_setup_wall_ms"), mainSetupWallMs},
        {QStringLiteral("sample_count"), requestedSamples},
        {QStringLiteral("samples"), sampleArray},
        {QStringLiteral("summary"), summary},
    };
    QSaveFile output(parser.value(outputOption));
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0 ||
        !output.commit()) {
        qCritical().noquote() << QStringLiteral("error: failed to write %1").arg(output.fileName());
        return 4;
    }
    qInfo().noquote() << QStringLiteral("PREFETCH_BENCHMARK samples=%1 output=%2")
                             .arg(requestedSamples)
                             .arg(output.fileName());
    return 0;
}
