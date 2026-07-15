#pragma once

#include "domain/ColumnCatalog.h"

#include <sqlite3.h>

#include <atomic>
#include <string>
#include <vector>

namespace ssa::infra::sqlite {

    void ensureDerivedCountSummary(sqlite3* db, const std::string& tableName,
                                   const std::vector<domain::ColumnDef>& columns,
                                   const std::atomic_bool* busyCancellationObserved);

} // namespace ssa::infra::sqlite
