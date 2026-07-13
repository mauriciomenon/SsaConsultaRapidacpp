#include "infra/export/CsvExportPort.h"
#include "ports/ISsaRepository.h"
#include "qt/FilesystemPath.h"

#include <QScopeGuard>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stop_token>
#include <system_error>

#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace {

    class FakeRepository : public ssa::ports::ISsaRepository {
      public:
        FakeRepository()
            : rows_{ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"descricao_ssa", "A,B"}}},
                    ssa::domain::SsaRecord{
                        {{"numero_ssa", "202500002"}, {"descricao_ssa", "Plain"}}}} {}

        explicit FakeRepository(std::vector<ssa::domain::SsaRecord> rows)
            : rows_(std::move(rows)) {}

        [[nodiscard]] ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                                      std::stop_token = {}) const override {
            ssa::domain::SsaPageResult result;
            result.pageIndex = request.pageIndex;
            result.pageSize = request.pageSize;
            result.totalRows = rows_.size();
            const auto start = request.pageIndex * request.pageSize;
            const auto end = std::min(start + request.pageSize, rows_.size());
            for (std::size_t index = start; index < end; ++index) {
                result.rows.push_back(rows_[index]);
            }
            return result;
        }

        [[nodiscard]] std::size_t count(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            (void)request;
            return rows_.size();
        }

        [[nodiscard]] std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& id, std::stop_token = {}) const override {
            (void)id;
            return std::nullopt;
        }
        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest& request,
                       std::stop_token = {}) const override {
            (void)request;
            return {};
        }

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        [[nodiscard]] ssa::ports::SsaReadResult
        readAll(const ssa::domain::SsaPageRequest& request, ssa::ports::SsaRecordConsumer consume,
                const std::stop_token stopToken = {}) const override {
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            std::size_t rowCount = 0;
            auto allRowsRequest = request;
            allRowsRequest.pageIndex = 0;
            while (true) {
                auto pageResult = page(allRowsRequest);
                for (const auto& row : pageResult.rows) {
                    if (auto error = consume(row); error.has_value()) {
                        return {rowCount, *error};
                    }
                    ++rowCount;
                }
                if (pageResult.rows.empty() || rowCount >= pageResult.totalRows) {
                    break;
                }
                ++allRowsRequest.pageIndex;
            }
            return {rowCount, {}};
        }

      private:
        std::vector<ssa::domain::SsaRecord> rows_;
    };

    class FailingAfterFirstRowRepository final : public FakeRepository {
      public:
        [[nodiscard]] ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                                        ssa::ports::SsaRecordConsumer consume,
                                                        const std::stop_token = {}) const override {
            const auto firstPage = page(request);
            REQUIRE_FALSE(firstPage.rows.empty());
            REQUIRE_FALSE(consume(firstPage.rows.front()).has_value());
            return {1, "simulated repository failure"};
        }
    };

    class RacingDestinationRepository final : public FakeRepository {
      public:
        explicit RacingDestinationRepository(std::filesystem::path outputPath)
            : outputPath_(std::move(outputPath)) {}

        [[nodiscard]] ssa::ports::SsaReadResult
        readAll(const ssa::domain::SsaPageRequest& request, ssa::ports::SsaRecordConsumer consume,
                const std::stop_token stopToken = {}) const override {
            std::ofstream competingOutput(outputPath_);
            competingOutput << "competing\n";
            competingOutput.close();
            return FakeRepository::readAll(request, std::move(consume), stopToken);
        }

      private:
        std::filesystem::path outputPath_;
    };

    class CancelAfterFirstRowRepository final : public FakeRepository {
      public:
        explicit CancelAfterFirstRowRepository(std::stop_source& stopSource)
            : stopSource_(stopSource) {}

        [[nodiscard]] ssa::ports::SsaReadResult
        readAll(const ssa::domain::SsaPageRequest& request, ssa::ports::SsaRecordConsumer consume,
                const std::stop_token stopToken = {}) const override {
            const auto firstPage = page(request);
            REQUIRE_FALSE(firstPage.rows.empty());
            REQUIRE_FALSE(consume(firstPage.rows.front()).has_value());
            stopSource_.request_stop();
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            return {1, {}};
        }

      private:
        std::stop_source& stopSource_;
    };

    class NonStandardFailureRepository final : public FakeRepository {
      public:
        [[nodiscard]] ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                                        ssa::ports::SsaRecordConsumer,
                                                        std::stop_token = {}) const override {
            throw 42;
        }
    };

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream input(path);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST_CASE("csv export port writes filtered list") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_test.csv";

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;
    request.query.visibleColumns = {"numero_ssa", "descricao_ssa"};

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(readFile(outputPath) == "No SSA,Descricao SSA\n202500001,\"A,B\"\n202500002,Plain\n");
}

TEST_CASE("csv export port preserves unicode output paths") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath = ssa::qt::toFileSystemPath(
        tempDir.filePath(QString::fromUtf8("exportacao-acao-\xE6\xBC\xA2.csv")));
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;
    request.query.visibleColumns = {"numero_ssa"};

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::is_regular_file(outputPath));
}

TEST_CASE("csv export port rejects existing output file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_existing.csv";
    {
        std::ofstream output(outputPath);
        output << "existing\n";
    }

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
}

TEST_CASE("csv export port rejects directory output") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = std::filesystem::path{tempDir.path().toStdString()};

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
}

TEST_CASE("csv export port exports complete filtered list from any current page") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_page.csv";

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;
    request.query.pageIndex = 1;
    request.query.pageSize = 1;
    request.query.visibleColumns = {"numero_ssa", "descricao_ssa"};

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    // Export must emit the COMPLETE filtered list regardless of the current page,
    // not just the rows of page 1. Both rows must appear in the file.
    REQUIRE(readFile(outputPath) == "No SSA,Descricao SSA\n202500001,\"A,B\"\n202500002,Plain\n");
}

TEST_CASE("csv export port propagates a stopped token to the repository") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_canceled.csv";
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = port.exportFilteredList(request, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE_FALSE(std::filesystem::exists(request.outputPath));
    REQUIRE(std::filesystem::is_empty(std::filesystem::path{tempDir.path().toStdString()}));
}

TEST_CASE("csv export port removes partial output after a repository failure") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<FailingAfterFirstRowRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_failed.csv";

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE_FALSE(std::filesystem::exists(request.outputPath));
    REQUIRE(std::filesystem::is_empty(std::filesystem::path{tempDir.path().toStdString()}));
}

#ifndef _WIN32
TEST_CASE("csv export port removes temporary output after a physical write failure") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    rlimit originalLimit{};
    REQUIRE(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    const auto previousSignalHandler = std::signal(SIGXFSZ, SIG_IGN);
    REQUIRE(previousSignalHandler != SIG_ERR);
    const auto restoreProcessLimit = qScopeGuard([&] {
        if (setrlimit(RLIMIT_FSIZE, &originalLimit) != 0) {
            std::abort();
        }
        std::signal(SIGXFSZ, previousSignalHandler);
    });
    auto limited = originalLimit;
    limited.rlim_cur = 32;
    REQUIRE(setrlimit(RLIMIT_FSIZE, &limited) == 0);

    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_write_failure.csv";

    const auto result = port.exportFilteredList(request);

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE((result.message.find("writing") != std::string::npos ||
             result.message.find("flushing") != std::string::npos));
    REQUIRE_FALSE(std::filesystem::exists(request.outputPath));
    REQUIRE(std::filesystem::is_empty(std::filesystem::path{tempDir.path().toStdString()}));
}
#endif

TEST_CASE("csv export port removes partial output after midstream cancellation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    std::stop_source stopSource;
    const auto repository = std::make_shared<CancelAfterFirstRowRepository>(stopSource);
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_canceled_mid.csv";

    const auto result = port.exportFilteredList(request, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE_FALSE(std::filesystem::exists(request.outputPath));
    REQUIRE(std::filesystem::is_empty(std::filesystem::path{tempDir.path().toStdString()}));
}

TEST_CASE("csv export port contains non standard repository failures") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto repository = std::make_shared<NonStandardFailureRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_unknown.csv";

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message == "unknown export failure");
    REQUIRE_FALSE(std::filesystem::exists(request.outputPath));
}

TEST_CASE("csv export port never replaces a destination created during export") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_race.csv";
    const auto repository = std::make_shared<RacingDestinationRepository>(outputPath);
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(readFile(outputPath) == "competing\n");
    REQUIRE(std::ranges::distance(std::filesystem::directory_iterator(tempDir.path().toStdString()),
                                  std::filesystem::directory_iterator{}) == 1);
}

TEST_CASE("csv export port neutralizes formula prefixes and leading controls") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const std::vector<std::string> values{"=SUM(A1)",
                                          "+1",
                                          "-1",
                                          "@cmd",
                                          "\t=SUM(A1)",
                                          "\r=SUM(A1)",
                                          "\n=SUM(A1)",
                                          "\v=SUM(A1)",
                                          "\xEF\xBC\x9D"
                                          "SUM(A1)",
                                          "\xEF\xBC\x8B"
                                          "1",
                                          "\xEF\xBC\x8D"
                                          "1",
                                          "\xEF\xBC\xA0"
                                          "cmd"};
    std::vector<ssa::domain::SsaRecord> rows;
    rows.reserve(values.size());
    for (const auto& value : values) {
        rows.push_back(ssa::domain::SsaRecord{{{"descricao_ssa", value}}});
    }
    const auto repository = std::make_shared<FakeRepository>(std::move(rows));
    ssa::infra::exporting::CsvExportPort port(repository);
    ssa::ports::ExportFilteredListRequest request;
    request.outputPath =
        std::filesystem::path{tempDir.path().toStdString()} / "ssa_cpp_export_formulas.csv";
    request.query.visibleColumns = {"descricao_ssa"};

    const auto result = port.exportFilteredList(request);
    const auto exported = readFile(request.outputPath);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    for (const auto& value : values) {
        REQUIRE(exported.find("\"'" + value + "\"\n") != std::string::npos);
    }
}
