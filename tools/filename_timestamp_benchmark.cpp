#include "domain/SsaImportPolicy.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

    constexpr std::string_view kCorpusVersion = "v1-numeric-noise-4096";
    constexpr int kDefaultSamples = 1;
    constexpr int kDefaultIterations = 100;
    constexpr int kMaxSamples = 1'000;
    constexpr int kMaxIterations = 100'000;
    constexpr int kWorkerStartTimeoutMs = 10'000;
    constexpr int kWorkerTimeoutMs = 30'000;
    constexpr std::array<std::string_view, 4> kCorpusCaseNames{
        "iso_at_start", "numeric_noise_then_iso", "numeric_noise_without_timestamp",
        "numeric_noise_then_day_first_ampm"};

    struct CorpusCase final {
        std::string name;
        std::string filename;
        std::string expected;
    };

    struct CaseSample final {
        std::string name;
        std::uint64_t wallNanoseconds{0};
        double perParseNanoseconds{0.0};
    };

    using WorkerSample = std::vector<CaseSample>;

    [[nodiscard]] std::vector<CorpusCase> corpus() {
        const std::string numericNoise(4'096, '7');
        return {
            {"iso_at_start", "2026-07-15.xlsx", "2026-07-15 00:00:00"},
            {"numeric_noise_then_iso", numericNoise + "_2026-07-15.xlsx", "2026-07-15 00:00:00"},
            {"numeric_noise_without_timestamp", numericNoise + "_invalid.xlsx", ""},
            {"numeric_noise_then_day_first_ampm", numericNoise + "_15-07-2026_0100PM.xlsx",
             "2026-07-15 13:00:00"}};
    }

    [[nodiscard]] std::optional<WorkerSample> runWorker(const int iterations, QString& error) {
        WorkerSample sample;
        const auto cases = corpus();
        sample.reserve(cases.size());
        for (const auto& item : cases) {
            std::uint64_t elapsedNanoseconds = 0;
            for (int index = 0; index < iterations; ++index) {
                const auto started = std::chrono::steady_clock::now();
                const auto actual =
                    ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp(item.filename);
                const auto finished = std::chrono::steady_clock::now();
                if (actual != item.expected) {
                    error = QStringLiteral("corpus case %1 returned an unexpected timestamp")
                                .arg(QString::fromStdString(item.name));
                    return std::nullopt;
                }
                const auto duration =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started)
                        .count();
                if (duration <= 0) {
                    error = QStringLiteral("clock did not produce a positive duration for %1")
                                .arg(QString::fromStdString(item.name));
                    return std::nullopt;
                }
                elapsedNanoseconds += static_cast<std::uint64_t>(duration);
            }
            sample.push_back(
                {item.name, elapsedNanoseconds,
                 static_cast<double>(elapsedNanoseconds) / static_cast<double>(iterations)});
        }
        return sample;
    }

    [[nodiscard]] QJsonObject workerJson(const WorkerSample& sample, const int iterations) {
        QJsonArray cases;
        for (const auto& item : sample) {
            cases.append(
                QJsonObject{{QStringLiteral("name"), QString::fromStdString(item.name)},
                            {QStringLiteral("wall_ns"), static_cast<qint64>(item.wallNanoseconds)},
                            {QStringLiteral("per_parse_ns"), item.perParseNanoseconds}});
        }
        return {{QStringLiteral("iterations"), iterations}, {QStringLiteral("cases"), cases}};
    }

    [[nodiscard]] std::optional<WorkerSample>
    parseWorkerSample(const QByteArray& payload, const int expectedIterations, QString& error) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("worker emitted invalid JSON: %1").arg(parseError.errorString());
            return std::nullopt;
        }
        const auto object = document.object();
        if (object.value(QStringLiteral("iterations")).toInt(-1) != expectedIterations) {
            error = QStringLiteral("worker metadata did not match the requested benchmark");
            return std::nullopt;
        }
        const auto caseArray = object.value(QStringLiteral("cases")).toArray();
        if (caseArray.size() != static_cast<qsizetype>(kCorpusCaseNames.size())) {
            error = QStringLiteral("worker emitted an incomplete corpus");
            return std::nullopt;
        }
        WorkerSample sample;
        sample.reserve(kCorpusCaseNames.size());
        for (qsizetype index = 0; index < caseArray.size(); ++index) {
            if (!caseArray.at(index).isObject()) {
                error = QStringLiteral("worker emitted an invalid corpus case");
                return std::nullopt;
            }
            const auto caseObject = caseArray.at(index).toObject();
            const auto wallValue = caseObject.value(QStringLiteral("wall_ns"));
            const auto perParseValue = caseObject.value(QStringLiteral("per_parse_ns"));
            const auto wallNanoseconds = wallValue.toVariant().toULongLong();
            const auto perParseNanoseconds = perParseValue.toDouble(-1.0);
            if (caseObject.value(QStringLiteral("name")).toString() !=
                    QString::fromUtf8(
                        kCorpusCaseNames[static_cast<std::size_t>(index)].data(),
                        static_cast<qsizetype>(
                            kCorpusCaseNames[static_cast<std::size_t>(index)].size())) ||
                !wallValue.isDouble() || wallNanoseconds == 0 || !perParseValue.isDouble() ||
                !std::isfinite(perParseNanoseconds) || perParseNanoseconds <= 0.0) {
                error = QStringLiteral("worker emitted invalid timing data");
                return std::nullopt;
            }
            sample.push_back({std::string{kCorpusCaseNames[static_cast<std::size_t>(index)]},
                              wallNanoseconds, perParseNanoseconds});
        }
        return sample;
    }

    [[nodiscard]] std::optional<WorkerSample> runChild(const int iterations, QString& error) {
        QProcess worker;
        worker.setProgram(QCoreApplication::applicationFilePath());
        worker.setArguments({QStringLiteral("--worker"), QStringLiteral("--iterations"),
                             QString::number(iterations)});
        worker.setProcessChannelMode(QProcess::SeparateChannels);
        worker.start();
        if (!worker.waitForStarted(kWorkerStartTimeoutMs)) {
            error = QStringLiteral("worker start failed: %1").arg(worker.errorString());
            return std::nullopt;
        }
        if (!worker.waitForFinished(kWorkerTimeoutMs)) {
            worker.kill();
            const bool terminatedAfterKill = worker.waitForFinished(kWorkerStartTimeoutMs);
            error = QStringLiteral("worker timed out after %1 ms").arg(kWorkerTimeoutMs);
            if (!terminatedAfterKill) {
                error += QStringLiteral("; process did not terminate within %1 ms after kill")
                             .arg(kWorkerStartTimeoutMs);
            }
            return std::nullopt;
        }
        if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0) {
            const auto stderrText = worker.readAllStandardError().trimmed();
            error = stderrText.isEmpty()
                        ? QStringLiteral("worker exited with code %1").arg(worker.exitCode())
                        : QStringLiteral("worker failed: %1").arg(QString::fromUtf8(stderrText));
            return std::nullopt;
        }
        return parseWorkerSample(worker.readAllStandardOutput(), iterations, error);
    }

    [[nodiscard]] double percentile(std::vector<double> values, const double fraction) {
        std::sort(values.begin(), values.end());
        const auto index =
            static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * fraction)) - 1;
        return values[index];
    }

    [[nodiscard]] QJsonObject summarize(const std::vector<WorkerSample>& samples) {
        QJsonObject cases;
        for (std::size_t caseIndex = 0; caseIndex < kCorpusCaseNames.size(); ++caseIndex) {
            std::vector<double> timings;
            timings.reserve(samples.size());
            for (const auto& sample : samples) {
                timings.push_back(sample[caseIndex].perParseNanoseconds);
            }
            cases.insert(
                QString::fromUtf8(kCorpusCaseNames[caseIndex].data(),
                                  static_cast<qsizetype>(kCorpusCaseNames[caseIndex].size())),
                QJsonObject{
                    {QStringLiteral("p50_per_parse_ns"), percentile(timings, 0.50)},
                    {QStringLiteral("p95_per_parse_ns"), percentile(std::move(timings), 0.95)}});
        }
        return cases;
    }

    [[nodiscard]] bool writeReport(const QString& outputPath, const QJsonObject& report,
                                   QString& error) {
        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly)) {
            error = QStringLiteral("cannot open output %1").arg(output.fileName());
            return false;
        }
        const auto payload = QJsonDocument(report).toJson(QJsonDocument::Indented);
        if (output.write(payload) != payload.size()) {
            output.cancelWriting();
            error = QStringLiteral("partial write to %1").arg(output.fileName());
            return false;
        }
        if (!output.commit()) {
            error = QStringLiteral("cannot commit output %1").arg(output.fileName());
            return false;
        }
        return true;
    }

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption samplesOption(
        QStringLiteral("samples"), QStringLiteral("Number of worker processes."),
        QStringLiteral("count"), QString::number(kDefaultSamples));
    const QCommandLineOption iterationsOption(
        QStringLiteral("iterations"), QStringLiteral("Parser calls per corpus case and worker."),
        QStringLiteral("count"), QString::number(kDefaultIterations));
    const QCommandLineOption outputOption(QStringLiteral("output"),
                                          QStringLiteral("Atomic JSON output path."),
                                          QStringLiteral("path"));
    const QCommandLineOption workerOption(QStringLiteral("worker"),
                                          QStringLiteral("Run one benchmark worker."));
    parser.addHelpOption();
    parser.addOption(samplesOption);
    parser.addOption(iterationsOption);
    parser.addOption(outputOption);
    parser.addOption(workerOption);
    parser.process(application);

    bool iterationsValid = false;
    const int iterations = parser.value(iterationsOption).toInt(&iterationsValid);
    if (!iterationsValid || iterations <= 0 || iterations > kMaxIterations ||
        !parser.positionalArguments().empty()) {
        qCritical("error: --iterations must be an integer from 1 to 100000");
        return 2;
    }
    if (parser.isSet(workerOption)) {
        QString error;
        const auto sample = runWorker(iterations, error);
        if (!sample) {
            qCritical().noquote() << QStringLiteral("error: %1").arg(error);
            return 3;
        }
        std::cout << QJsonDocument(workerJson(*sample, iterations))
                         .toJson(QJsonDocument::Compact)
                         .toStdString()
                  << '\n';
        return 0;
    }

    bool samplesValid = false;
    const int requestedSamples = parser.value(samplesOption).toInt(&samplesValid);
    if (!samplesValid || requestedSamples <= 0 || requestedSamples > kMaxSamples ||
        !parser.isSet(outputOption)) {
        qCritical("error: --samples must be an integer from 1 to 1000 and --output is required");
        return 2;
    }

    std::vector<WorkerSample> samples;
    QJsonArray rawSamples;
    samples.reserve(static_cast<std::size_t>(requestedSamples));
    for (int index = 0; index < requestedSamples; ++index) {
        QString error;
        const auto sample = runChild(iterations, error);
        if (!sample) {
            qCritical().noquote() << QStringLiteral("error: worker %1 of %2 failed: %3")
                                         .arg(index + 1)
                                         .arg(requestedSamples)
                                         .arg(error);
            return 4;
        }
        auto sampleObject = workerJson(*sample, iterations);
        sampleObject.insert(QStringLiteral("sample_index"), index);
        rawSamples.append(sampleObject);
        samples.push_back(*sample);
    }

    const QJsonObject report{
        {QStringLiteral("corpus_version"),
         QString::fromUtf8(kCorpusVersion.data(), static_cast<qsizetype>(kCorpusVersion.size()))},
        {QStringLiteral("process_count"), requestedSamples},
        {QStringLiteral("iterations"), iterations},
        {QStringLiteral("normalizations_per_process"),
         static_cast<qint64>(iterations * static_cast<int>(kCorpusCaseNames.size()))},
        {QStringLiteral("samples"), rawSamples},
        {QStringLiteral("per_parse_ns"), summarize(samples)}};
    QString error;
    if (!writeReport(parser.value(outputOption), report, error)) {
        qCritical().noquote() << QStringLiteral("error: %1").arg(error);
        return 5;
    }
    std::cout << "SSA_FILENAME_TIMESTAMP_BENCHMARK_SUMMARY "
              << QJsonDocument(report).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
}
