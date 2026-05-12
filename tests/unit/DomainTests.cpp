#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("column catalog exposes visible and general-search contracts") {
    const auto visible = ssa::domain::ColumnCatalog::defaultVisibleKeys();
    const auto search = ssa::domain::ColumnCatalog::generalSearchKeys();

    REQUIRE_FALSE(visible.empty());
    REQUIRE_FALSE(search.empty());
    REQUIRE(ssa::domain::ColumnCatalog::filterColumnKeys().front() == "situacao");
    REQUIRE(ssa::domain::ColumnCatalog::defaultFilterColumnKey() == "situacao");
    REQUIRE(ssa::domain::ColumnCatalog::contains("numero_ssa"));
    REQUIRE(ssa::domain::ColumnCatalog::contains("situacao"));
    REQUIRE_FALSE(ssa::domain::ColumnCatalog::contains("unknown_column"));
}

TEST_CASE("ssa record returns empty string for missing values") {
    ssa::domain::SsaRecord record;
    record.values["numero_ssa"] = "202500001";

    REQUIRE(record.valueOf("numero_ssa") == "202500001");
    REQUIRE(record.valueOf("missing").empty());
}
