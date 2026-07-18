#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTablePageFormatter.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#ifdef _WIN32
// clang-format off
#include <windows.h>
// clang-format on
#else
#include <sys/resource.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

    constexpr int kDefaultSamples = 1;
    constexpr int kDefaultIterations = 10;
    constexpr int kMaxSamples = 1'000;
    constexpr int kMaxIterations = 1'000;
    constexpr std::size_t kRows = ssa::domain::kMaxPageSize;
    constexpr std::array<std::string_view, 12> kColumnKeys{
        "numero_ssa",       "situacao",           "descricao_ssa",      "data_cadastro",
        "qtd_derivadas",    "semana_cadastro",    "prazo_limite",       "semana_programada",
        "semana_executada", "num_reprogramacoes", "descricao_execucao", "data_arquivo_origem"};

    struct Sample final {
        double wallMilliseconds{0.0};
        double cpuMilliseconds{0.0};
        std::size_t formattedValuesTotal{0};
    };

    [[nodiscard]] std::optional<std::uint64_t> processCpuNanoseconds() {
#ifdef _WIN32
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0) {
            return std::nullopt;
        }
        ULARGE_INTEGER kernelValue{};
        kernelValue.LowPart = kernel.dwLowDateTime;
        kernelValue.HighPart = kernel.dwHighDateTime;
        ULARGE_INTEGER userValue{};
        userValue.LowPart = user.dwLowDateTime;
        userValue.HighPart = user.dwHighDateTime;
        return (kernelValue.QuadPart + userValue.QuadPart) * 100ULL;
#else
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return std::nullopt;
        }
        const auto nanoseconds = [](const timeval value) {
            return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL +
                   static_cast<std::uint64_t>(value.tv_usec) * 1'000ULL;
        };
        return nanoseconds(usage.ru_utime) + nanoseconds(usage.ru_stime);
#endif
    }

    [[nodiscard]] std::vector<std::string> displayKeys() {
        std::vector<std::string> keys;
        keys.reserve(kColumnKeys.size());
        for (const auto key : kColumnKeys) {
            keys.emplace_back(key);
        }
        return keys;
    }

    [[nodiscard]] ssa::domain::SsaPageResult fixturePage() {
        auto mutableSchema = std::make_shared<ssa::domain::SsaRecord::SchemaIndex>();
        mutableSchema->keys = displayKeys();
        mutableSchema->indexByKey.reserve(mutableSchema->keys.size());
        for (std::size_t index = 0; index < mutableSchema->keys.size(); ++index) {
            mutableSchema->indexByKey.emplace(mutableSchema->keys[index], index);
        }
        const std::shared_ptr<const ssa::domain::SsaRecord::SchemaIndex> schema =
            std::move(mutableSchema);

        ssa::domain::SsaPageResult page;
        page.rows.reserve(kRows);
        for (std::size_t row = 0; row < kRows; ++row) {
            const auto rowNumber = std::to_string(202'600'000 + row);
            const auto week = std::to_string(202601 + (row % 52));
            page.rows.emplace_back(
                schema, std::vector<std::string>{
                            rowNumber, row % 4 == 0 ? "APV" : "EME",
                            "Descricao da SSA " + rowNumber + " para medicao de pagina",
                            "2026-07-18", std::to_string(row % 7), week, "2026-08-15", week, week,
                            std::to_string(row % 3), "Execucao registrada para a SSA " + rowNumber,
                            "2026-07-18"});
        }
        page.totalRows = page.rows.size();
        page.pageSize = page.rows.size();
        return page;
    }

    [[nodiscard]] double milliseconds(const std::uint64_t nanoseconds) {
        return static_cast<double>(nanoseconds) / 1'000'000.0;
    }

    [[nodiscard]] std::optional<Sample>
    measure(const ssa::domain::SsaPageResult& page,
            const std::vector<ssa::presentation::SsaDisplayColumn>& displayColumns,
            const int iterations, QString& error) {
        const auto cpuStarted = processCpuNanoseconds();
        const auto wallStarted = std::chrono::steady_clock::now();
        std::size_t formattedValues = 0;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            auto formatted =
                ssa::presentation::SsaTablePageFormatter::format(page, displayColumns, {});
            if (!formatted || !formatted->hasValidShape() ||
                formatted->rowCount != page.rows.size() ||
                formatted->columnCount != displayColumns.size() ||
                formatted->values.size() != page.rows.size() * displayColumns.size()) {
                error = QStringLiteral("formatter did not preserve the page shape");
                return std::nullopt;
            }
            if (formatted->values.at(3).toString() != QStringLiteral("18/07/2026")) {
                error = QStringLiteral("formatter did not format the date fixture");
                return std::nullopt;
            }
            formattedValues += formatted->values.size();
        }
        const auto wallFinished = std::chrono::steady_clock::now();
        const auto cpuFinished = processCpuNanoseconds();
        if (!cpuStarted || !cpuFinished || *cpuFinished < *cpuStarted) {
            error = QStringLiteral("CPU metric is unavailable");
            return std::nullopt;
        }
        const auto wallNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(wallFinished - wallStarted)
                .count());
        const auto iterationsAsDouble = static_cast<double>(iterations);
        return Sample{milliseconds(wallNanoseconds) / iterationsAsDouble,
                      milliseconds(*cpuFinished - *cpuStarted) / iterationsAsDouble,
                      formattedValues};
    }

    [[nodiscard]] double percentile(std::vector<double> values, const double fraction) {
        std::sort(values.begin(), values.end());
        const auto index =
            static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * fraction)) - 1;
        return values[index];
    }

    [[nodiscard]] QJsonObject sampleJson(const Sample& sample, const int iterations) {
        return {{QStringLiteral("wall_ms"), sample.wallMilliseconds},
                {QStringLiteral("cpu_ms"), sample.cpuMilliseconds},
                {QStringLiteral("formatted_values_total"),
                 static_cast<qint64>(sample.formattedValuesTotal)},
                {QStringLiteral("iterations"), iterations}};
    }

    [[nodiscard]] QJsonObject summaryJson(const std::vector<Sample>& samples) {
        std::vector<double> wallMilliseconds;
        std::vector<double> cpuMilliseconds;
        wallMilliseconds.reserve(samples.size());
        cpuMilliseconds.reserve(samples.size());
        for (const auto& sample : samples) {
            wallMilliseconds.push_back(sample.wallMilliseconds);
            cpuMilliseconds.push_back(sample.cpuMilliseconds);
        }
        return {
            {QStringLiteral("wall_ms"),
             QJsonObject{{QStringLiteral("p50"), percentile(wallMilliseconds, 0.50)},
                         {QStringLiteral("p95"), percentile(std::move(wallMilliseconds), 0.95)}}},
            {QStringLiteral("cpu_ms"),
             QJsonObject{{QStringLiteral("p50"), percentile(cpuMilliseconds, 0.50)},
                         {QStringLiteral("p95"), percentile(std::move(cpuMilliseconds), 0.95)}}}};
    }

    [[nodiscard]] bool writeReport(const QString& path, const QJsonObject& report, QString& error) {
        QSaveFile output(path);
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
        QStringLiteral("samples"), QStringLiteral("Number of measurements."),
        QStringLiteral("count"), QString::number(kDefaultSamples));
    const QCommandLineOption iterationsOption(
        QStringLiteral("iterations"), QStringLiteral("Formatting runs per measurement."),
        QStringLiteral("count"), QString::number(kDefaultIterations));
    const QCommandLineOption outputOption(QStringLiteral("output"),
                                          QStringLiteral("Atomic JSON output path."),
                                          QStringLiteral("path"));
    parser.addHelpOption();
    parser.addOption(samplesOption);
    parser.addOption(iterationsOption);
    parser.addOption(outputOption);
    parser.process(application);

    bool samplesValid = false;
    const int samples = parser.value(samplesOption).toInt(&samplesValid);
    bool iterationsValid = false;
    const int iterations = parser.value(iterationsOption).toInt(&iterationsValid);
    if (!samplesValid || samples <= 0 || samples > kMaxSamples || !iterationsValid ||
        iterations <= 0 || iterations > kMaxIterations || !parser.isSet(outputOption) ||
        !parser.positionalArguments().empty()) {
        qCritical(
            "error: --samples and --iterations must be from 1 to 1000, and --output is required");
        return 2;
    }

    const auto page = fixturePage();
    const auto displayColumns =
        ssa::presentation::SsaColumnDisplayCatalog{}.resolveAll(displayKeys());
    QString error;
    if (!measure(page, displayColumns, 1, error)) {
        qCritical().noquote() << QStringLiteral("error: warmup failed: %1").arg(error);
        return 3;
    }

    std::vector<Sample> samplesData;
    QJsonArray rawSamples;
    samplesData.reserve(static_cast<std::size_t>(samples));
    for (int index = 0; index < samples; ++index) {
        const auto sample = measure(page, displayColumns, iterations, error);
        if (!sample) {
            qCritical().noquote() << QStringLiteral("error: sample %1 of %2 failed: %3")
                                         .arg(index + 1)
                                         .arg(samples)
                                         .arg(error);
            return 4;
        }
        auto json = sampleJson(*sample, iterations);
        json.insert(QStringLiteral("sample_index"), index);
        rawSamples.append(json);
        samplesData.push_back(*sample);
    }

    const QJsonObject report{
        {QStringLiteral("scope"), QStringLiteral("formatter_only_prebuilt_500x12_page")},
        {QStringLiteral("rows"), static_cast<qint64>(page.rows.size())},
        {QStringLiteral("columns"), static_cast<qint64>(displayColumns.size())},
        {QStringLiteral("samples"), samples},
        {QStringLiteral("iterations_per_sample"), iterations},
        {QStringLiteral("summary"), summaryJson(samplesData)},
        {QStringLiteral("raw_samples"), rawSamples}};
    if (!writeReport(parser.value(outputOption), report, error)) {
        qCritical().noquote() << QStringLiteral("error: %1").arg(error);
        return 5;
    }

    const auto summary = report.value(QStringLiteral("summary")).toObject();
    const auto wallP95 =
        summary.value(QStringLiteral("wall_ms")).toObject().value(QStringLiteral("p95"));
    const auto cpuP95 =
        summary.value(QStringLiteral("cpu_ms")).toObject().value(QStringLiteral("p95"));
    std::cout << "SSA_TABLE_PAGE_FORMAT_BENCHMARK rows=" << page.rows.size()
              << " columns=" << displayColumns.size() << " samples=" << samples
              << " iterations=" << iterations << " wall_p95_ms=" << wallP95.toDouble()
              << " cpu_p95_ms=" << cpuP95.toDouble() << '\n';
    return 0;
}
