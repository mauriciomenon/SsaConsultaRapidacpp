#pragma once

#include "SsaCliController.h"

#include <filesystem>
#include <memory>

namespace ssa::infra::sqlite {
    class SqliteSsaRepository;
}

namespace ssa::app::cli {

    class CliServiceFactory final {
      public:
        [[nodiscard]] SsaCliController createController();

      private:
        [[nodiscard]] std::shared_ptr<ssa::infra::sqlite::SqliteSsaRepository>
        repositoryForPath(const std::filesystem::path& dbPath) const;
    };

} // namespace ssa::app::cli
