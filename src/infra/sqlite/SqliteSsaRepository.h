#pragma once

#include "infra/sqlite/SqliteConnection.h"
#include "ports/ISsaRepository.h"
#include "query/SqlQueryBuilder.h"

#include <filesystem>
#include <optional>
#include <stop_token>

namespace ssa::infra::sqlite {

    // Synchronous storage port implementation.
    // GUI callers must use presentation::PageQueryCoordinator or another worker boundary.
    class SqliteSsaRepository final : public ports::ISsaRepository {
      public:
        explicit SqliteSsaRepository(std::filesystem::path dbPath);
        SqliteSsaRepository(std::filesystem::path dbPath, query::SqlQueryBuilder queryBuilder);

        [[nodiscard]] domain::SsaPageResult page(const domain::SsaPageRequest& request,
                                                 std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::size_t count(const domain::SsaPageRequest& request,
                                        std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::optional<domain::SsaRecord>
        recordBySsaNumber(const domain::SsaNumber& number,
                          std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::vector<domain::SsaDerivadaEntry>
        derivadasDiretas(const domain::SsaNumber& number,
                         std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request,
                       std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::size_t maxValueLength(std::string_view columnKey,
                                                 std::stop_token stopToken = {}) const override;

        [[nodiscard]] ports::SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                   ports::SsaRecordConsumer consume,
                                                   std::stop_token stopToken = {}) const override;

      private:
        [[nodiscard]] static std::size_t executeCount(sqlite3* db, const query::SqlQuery& query,
                                                      const std::stop_token& stopToken,
                                                      const std::atomic_bool* busyCanceled);
        [[nodiscard]] static std::vector<domain::SsaRecord>
        executeRows(sqlite3* db, const query::SqlQuery& query, const std::stop_token& stopToken,
                    const std::atomic_bool* busyCanceled);
        [[nodiscard]] static ports::SsaReadResult
        consumeRows(sqlite3* db, const query::SqlQuery& query,
                    const ports::SsaRecordConsumer& consume, const std::stop_token& stopToken,
                    const std::atomic_bool* busyCanceled);

        std::filesystem::path dbPath_;
        query::SqlQueryBuilder queryBuilder_;
    };

} // namespace ssa::infra::sqlite
