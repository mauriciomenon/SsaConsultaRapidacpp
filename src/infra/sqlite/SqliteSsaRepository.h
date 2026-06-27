#pragma once

#include "infra/sqlite/SqliteConnection.h"
#include "ports/ISsaRepository.h"
#include "query/SqlQueryBuilder.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

namespace ssa::infra::sqlite {

    // Synchronous storage port implementation.
    // GUI callers must use presentation::PageQueryCoordinator or another worker boundary.
    class SqliteSsaRepository final : public ports::ISsaRepository {
      public:
        explicit SqliteSsaRepository(std::filesystem::path dbPath);
        SqliteSsaRepository(std::filesystem::path dbPath, query::SqlQueryBuilder queryBuilder);

        [[nodiscard]] domain::SsaPageResult
        page(const domain::SsaPageRequest& request) const override;

        [[nodiscard]] std::size_t count(const domain::SsaPageRequest& request) const override;

        [[nodiscard]] std::optional<domain::SsaRecord>
        recordBySsaNumber(const domain::SsaNumber& number) const override;

        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request) const override;

        [[nodiscard]] ports::SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                   ports::SsaRecordConsumer consume) const override;

      private:
        [[nodiscard]] SqliteConnection& connectionLocked(const std::scoped_lock<std::mutex>&) const;
        [[nodiscard]] static std::size_t executeCount(sqlite3* db, const query::SqlQuery& query);
        [[nodiscard]] static std::vector<domain::SsaRecord>
        executeRows(sqlite3* db, const query::SqlQuery& query);
        [[nodiscard]] static ports::SsaReadResult
        consumeRows(sqlite3* db, const query::SqlQuery& query,
                    const ports::SsaRecordConsumer& consume);

        std::filesystem::path dbPath_;
        query::SqlQueryBuilder queryBuilder_;
        mutable std::mutex connectionMutex_;
        mutable std::unique_ptr<SqliteConnection> connection_;
    };

} // namespace ssa::infra::sqlite
