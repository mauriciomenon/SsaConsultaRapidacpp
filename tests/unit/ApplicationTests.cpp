#include "application/SsaBrowseService.h"
#include "application/SsaWorkflowService.h"
#include "application/UnavailableWorkflowPort.h"
#include "ports/ISsaRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace {

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        [[nodiscard]] ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                                      std::stop_token = {}) const override {
            lastRequest = request;
            ssa::domain::SsaPageResult result;
            result.pageIndex = request.pageIndex;
            result.pageSize = request.pageSize;
            result.totalRows = 1;
            result.rows.push_back(ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}}});
            return result;
        }

        [[nodiscard]] std::size_t count(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            lastRequest = request;
            return 1;
        }

        [[nodiscard]] std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& id, std::stop_token = {}) const override {
            if (id.value() != "202500001") {
                return std::nullopt;
            }
            return ssa::domain::SsaRecord{{{"numero_ssa", id.value()}}};
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

        [[nodiscard]] ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                                        ssa::ports::SsaRecordConsumer consume,
                                                        std::stop_token = {}) const override {
            auto pageResult = page(request);
            std::size_t rowCount = 0;
            for (const auto& row : pageResult.rows) {
                if (auto error = consume(row); error.has_value()) {
                    return {rowCount, *error};
                }
                ++rowCount;
            }
            return {rowCount, {}};
        }

        mutable ssa::domain::SsaPageRequest lastRequest;
    };

} // namespace

TEST_CASE("browse service normalizes empty visible columns and page size") {
    const auto repository = std::make_shared<FakeRepository>();
    const auto query = std::make_shared<ssa::query::SsaQueryService>(repository);
    const ssa::application::SsaBrowseService service(query);

    ssa::domain::SsaPageRequest request;
    request.pageSize = 0;

    const auto page = service.page(request);

    REQUIRE(page.pageSize == static_cast<std::size_t>(ssa::domain::kMinPageSize));
    REQUIRE_FALSE(repository->lastRequest.visibleColumns.empty());
}

TEST_CASE("browse service clamps oversized page size without narrowing") {
    const auto repository = std::make_shared<FakeRepository>();
    const auto query = std::make_shared<ssa::query::SsaQueryService>(repository);
    const ssa::application::SsaBrowseService service(query);

    ssa::domain::SsaPageRequest request;
    request.pageSize = std::numeric_limits<std::size_t>::max();

    const auto page = service.page(request);

    REQUIRE(page.pageSize == static_cast<std::size_t>(ssa::domain::kMaxPageSize));
    REQUIRE(repository->lastRequest.pageSize ==
            static_cast<std::size_t>(ssa::domain::kMaxPageSize));
}

TEST_CASE("browse service returns details by SSA number") {
    const auto repository = std::make_shared<FakeRepository>();
    const auto query = std::make_shared<ssa::query::SsaQueryService>(repository);
    const ssa::application::SsaBrowseService service(query);

    const auto record = service.details("202500001");

    REQUIRE(record.has_value());
    REQUIRE(record->valueOf("numero_ssa") == "202500001");
}

TEST_CASE("browse service validates requested visible columns") {
    const auto repository = std::make_shared<FakeRepository>();
    const auto query = std::make_shared<ssa::query::SsaQueryService>(repository);
    const ssa::application::SsaBrowseService service(query);

    REQUIRE_THROWS_AS(service.columnsOrDefault({"missing_column"}), std::invalid_argument);
    REQUIRE(service.columnsOrDefault({"numero_ssa"}).front() == "numero_ssa");
}

TEST_CASE("browse service validates requested sort column") {
    const auto repository = std::make_shared<FakeRepository>();
    const auto query = std::make_shared<ssa::query::SsaQueryService>(repository);
    const ssa::application::SsaBrowseService service(query);

    ssa::domain::SsaPageRequest request;
    request.sort.columnKey = "missing_column";

    REQUIRE_THROWS_AS(service.page(request), std::invalid_argument);
}

TEST_CASE("workflow service reports missing adapters explicitly") {
    const ssa::application::SsaWorkflowService workflows;

    const auto rescan = workflows.rescan({});
    const auto exportResult = workflows.exportFilteredList({});
    const auto derivadas = workflows.syncDerivadas();

    REQUIRE(rescan.status == ssa::ports::WorkflowStatus::NotImplemented);
    REQUIRE(exportResult.status == ssa::ports::WorkflowStatus::NotImplemented);
    REQUIRE(derivadas.status == ssa::ports::WorkflowStatus::NotImplemented);
}

TEST_CASE("unavailable workflow adapter reports not implemented explicitly") {
    auto unavailable = std::make_shared<ssa::application::UnavailableWorkflowPort>();
    const ssa::application::SsaWorkflowService workflows(unavailable, unavailable, unavailable,
                                                         unavailable);

    REQUIRE(workflows.rescan({}).status == ssa::ports::WorkflowStatus::NotImplemented);
    REQUIRE(workflows.resetDatabase().status == ssa::ports::WorkflowStatus::NotImplemented);
    REQUIRE(workflows.syncDerivadas().status == ssa::ports::WorkflowStatus::NotImplemented);
}
