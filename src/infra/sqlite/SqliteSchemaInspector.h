#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ssa::infra::sqlite {

    class SqliteSchemaInspector final {
      public:
        explicit SqliteSchemaInspector(std::filesystem::path dbPath);

        [[nodiscard]] bool hasSsaTable() const;
        [[nodiscard]] std::vector<std::string> ssaColumns() const;

      private:
        std::filesystem::path dbPath_;
    };

} // namespace ssa::infra::sqlite
