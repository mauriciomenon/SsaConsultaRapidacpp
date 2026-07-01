#include "domain/ColumnCatalog.h"
#include "domain/ColumnValuePriorityPolicy.h"
#include "domain/SectorHierarchy.h"
#include "domain/SsaRelationGraph.h"
#include "domain/SsaTypes.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("column catalog exposes visible and general-search contracts") {
    const auto visible = ssa::domain::ColumnCatalog::defaultVisibleKeys();
    const auto search = ssa::domain::ColumnCatalog::generalSearchKeys();

    REQUIRE_FALSE(visible.empty());
    REQUIRE_FALSE(search.empty());
    REQUIRE(ssa::domain::ColumnCatalog::orderedFilterColumnKeys().front() == "situacao");
    REQUIRE(ssa::domain::ColumnCatalog::defaultFilterColumnKey() == "situacao");
    REQUIRE(ssa::domain::ColumnCatalog::contains("numero_ssa"));
    REQUIRE(ssa::domain::ColumnCatalog::contains("situacao"));
    REQUIRE(ssa::domain::ColumnCatalog::contains("qtd_derivadas"));
    REQUIRE_FALSE(ssa::domain::ColumnCatalog::contains("unknown_column"));

    REQUIRE(std::ranges::find(visible, "qtd_derivadas") != visible.end());
    REQUIRE(std::ranges::find(search, "qtd_derivadas") == search.end());
    REQUIRE(
        std::ranges::find(ssa::domain::ColumnCatalog::orderedFilterColumnKeys(), "qtd_derivadas") ==
        ssa::domain::ColumnCatalog::orderedFilterColumnKeys().end());

    const auto storage = ssa::domain::ColumnCatalog::storageColumns();
    REQUIRE(std::ranges::none_of(storage, [](const ssa::domain::ColumnDef& column) {
        return column.key == "qtd_derivadas";
    }));
}

TEST_CASE("column catalog exposes expanded advanced filter fields") {
    const auto keys = ssa::domain::ColumnCatalog::advancedFilterKeys();
    const auto containsKey = [&keys](const std::string_view key) {
        return std::ranges::find(keys, key) != keys.end();
    };

    REQUIRE(keys.size() >= 18);
    REQUIRE(containsKey("setor_emissor"));
    REQUIRE(containsKey("setor_executor"));
    REQUIRE(containsKey("solicitante"));
    REQUIRE(containsKey("responsavel_programacao"));
    REQUIRE(containsKey("responsavel_execucao"));
    REQUIRE(containsKey("status_execucao_prazo"));
    REQUIRE(containsKey("situacao_da_parcial"));
    for (const auto key : keys) {
        REQUIRE(ssa::domain::ColumnCatalog::contains(key));
    }
}

TEST_CASE("column value priority policy recognizes SMIN and SMME prefixes") {
    CHECK(ssa::domain::isPriorityColumnValue("IEE3"));
    CHECK(ssa::domain::isPriorityColumnValue("MEL4"));
    CHECK(ssa::domain::isPriorityColumnValue("SMIN.DT"));
    CHECK(ssa::domain::isPriorityColumnValue("smme"));
    CHECK_FALSE(ssa::domain::isPriorityColumnValue("SMINX"));
    CHECK_FALSE(ssa::domain::isPriorityColumnValue("MEG2"));
}

TEST_CASE("column value priority policy orders display values") {
    std::vector<std::string> values{"MEL3",  "ANA",  "IEE2", "IEE3", "MEL1",
                                    "BRUNO", "IEE4", "IEE1", "MEG2"};
    std::ranges::sort(values, ssa::domain::columnValueLessForDisplay);

    REQUIRE(values == std::vector<std::string>{"IEE3", "IEE1", "IEE2", "IEE4", "MEL1", "MEL3",
                                               "ANA", "BRUNO", "MEG2"});
}

TEST_CASE("column value priority policy orders responsible names and numeric values") {
    std::vector<std::string> people{"MARIA",     "IEE2 BRUNO", "MEL1 CAIO", "IEE3 ANA",
                                    "IEE1 DORA", "MEL4 EVA",   "IEE4 BIA"};
    std::ranges::sort(people, ssa::domain::columnValueLessForDisplay);

    REQUIRE(people == std::vector<std::string>{"IEE3 ANA", "IEE1 DORA", "IEE2 BRUNO", "IEE4 BIA",
                                               "MEL1 CAIO", "MEL4 EVA", "MARIA"});

    std::vector<std::string> numbers{"1", "12", "2", "9", "7", "3"};
    std::ranges::sort(numbers, ssa::domain::columnValueLessForDisplay);

    REQUIRE(numbers == std::vector<std::string>{"1", "2", "3", "7", "9", "12"});
}

TEST_CASE("sector hierarchy expands divisions and orders known sectors") {
    const auto smin = ssa::domain::SectorHierarchy::sectorsForDivision("smin");
    REQUIRE(smin == std::vector<std::string>{"IEE1", "IEE2", "IEE3", "IEE4"});
    REQUIRE(ssa::domain::SectorHierarchy::divisionForSector("meg2") == "SMMG");

    const std::vector<std::string> mixed{"MEG2", "IEE4", "MEL1", "IEE1", "IEE1"};
    const auto ordered = ssa::domain::SectorHierarchy::orderedSectors(mixed);
    REQUIRE(ordered == std::vector<std::string>{"MEL1", "IEE1", "IEE4", "MEG2"});
}

TEST_CASE("ssa record returns empty string for missing values") {
    const ssa::domain::SsaRecord record{{{"numero_ssa", "202500001"}}};

    REQUIRE(record.valueOf("numero_ssa") == "202500001");
    REQUIRE(record.valueOf("missing").empty());
}

TEST_CASE("ssa relation graph exposes current and linked SSAs only when links exist") {
    const ssa::domain::SsaRecord root{{{"numero_ssa", "202600001"}}};
    const auto rootRelations = ssa::domain::SsaRelationGraph::fromRecord(root);
    REQUIRE(rootRelations.size() == 1);
    CHECK(rootRelations[0].kind == ssa::domain::SsaRelationKind::Current);
    CHECK(rootRelations[0].number == "202600001");

    const ssa::domain::SsaRecord linked{{{"numero_ssa", "202600002"},
                                         {"derivada_de", "202600001"},
                                         {"numero_ssa_relacionada_1", "202600003"}}};
    const auto relations = ssa::domain::SsaRelationGraph::fromRecord(linked);

    REQUIRE(relations.size() == 3);
    CHECK(relations[0].kind == ssa::domain::SsaRelationKind::Current);
    CHECK(relations[0].number == "202600002");
    CHECK(relations[1].kind == ssa::domain::SsaRelationKind::DerivedFrom);
    CHECK(relations[1].number == "202600001");
    CHECK(relations[2].kind == ssa::domain::SsaRelationKind::Related);
    CHECK(relations[2].number == "202600003");
}

TEST_CASE("ssa record rejects schema and value count mismatch") {
    auto schema = std::make_shared<ssa::domain::SsaRecord::SchemaIndex>();
    schema->keys = {"numero_ssa", "situacao"};
    schema->indexByKey.emplace("numero_ssa", 0);
    schema->indexByKey.emplace("situacao", 1);

    REQUIRE_THROWS_AS(ssa::domain::SsaRecord(schema, std::vector<std::string>{"202500001"}),
                      std::invalid_argument);
}

TEST_CASE("page count rejects zero page size") {
    REQUIRE_THROWS_AS(ssa::domain::pageCount(10, 0), std::invalid_argument);
}
