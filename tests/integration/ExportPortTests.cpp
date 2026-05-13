#include "infra/export/CsvExportPort.h"
#include "ports/ISsaRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        [[nodiscard]] ssa::domain::SsaPageResult
        page(const ssa::domain::SsaPageRequest& request) const override {
            ssa::domain::SsaPageResult result;
            result.pageIndex = request.pageIndex;
            result.pageSize = request.pageSize;
            result.totalRows = 2;
            const std::vector<ssa::domain::SsaRecord> rows{
                ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"descricao_ssa", "A,B"}}},
                ssa::domain::SsaRecord{{{"numero_ssa", "202500002"}, {"descricao_ssa", "Plain"}}}};
            const auto start = request.pageIndex * request.pageSize;
            const auto end = std::min(start + request.pageSize, rows.size());
            for (std::size_t index = start; index < end; ++index) {
                result.rows.push_back(rows[index]);
            }
            return result;
        }

        [[nodiscard]] std::size_t count(const ssa::domain::SsaPageRequest& request) const override {
            (void)request;
            return 2;
        }

        [[nodiscard]] std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& id) const override {
            (void)id;
            return std::nullopt;
        }

        [[nodiscard]] std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest& request) const override {
            (void)request;
            return {};
        }

        [[nodiscard]] ssa::ports::SsaReadResult
        readAll(const ssa::domain::SsaPageRequest& request,
                ssa::ports::SsaRecordConsumer consume) const override {
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
    };

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream input(path);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

} // namespace

TEST_CASE("csv export port writes filtered list") {
    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath = std::filesystem::temp_directory_path() / "ssa_cpp_export_test.csv";
    std::filesystem::remove(outputPath);

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;
    request.query.visibleColumns = {"numero_ssa", "descricao_ssa"};

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(readFile(outputPath) == "No SSA,Descricao SSA\n202500001,\"A,B\"\n202500002,Plain\n");
    std::filesystem::remove(outputPath);
}

TEST_CASE("csv export port rejects existing output file") {
    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath = std::filesystem::temp_directory_path() / "ssa_cpp_export_existing.csv";
    {
        std::ofstream output(outputPath);
        output << "existing\n";
    }

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    std::filesystem::remove(outputPath);
}

TEST_CASE("csv export port rejects directory output") {
    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = std::filesystem::temp_directory_path();

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
}

TEST_CASE("csv export port exports complete filtered list from any current page") {
    const auto repository = std::make_shared<FakeRepository>();
    ssa::infra::exporting::CsvExportPort port(repository);
    const auto outputPath = std::filesystem::temp_directory_path() / "ssa_cpp_export_page.csv";
    std::filesystem::remove(outputPath);

    ssa::ports::ExportFilteredListRequest request;
    request.outputPath = outputPath;
    request.query.pageIndex = 1;

    const auto result = port.exportFilteredList(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    std::filesystem::remove(outputPath);
}
