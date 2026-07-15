#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "infra/import/SsaSpreadsheetMapper.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>
#include <utility>

TEST_CASE("SSA spreadsheet header catalog preserves the Python import contract") {
    using Catalog = ssa::infra::importing::SsaSpreadsheetHeaderCatalog;
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 12> samples{
        std::pair{"Nº SSA*", "numero_ssa"},
        std::pair{"process_status", "situacao"},
        std::pair{"Fecha de Emision", "data_cadastro"},
        std::pair{"Prazo Limite", "prazo_limite"},
        std::pair{"Tempo TPE Plan.", "total_tempo_tpe_planejado"},
        std::pair{"Total Tempo TEX Executada", "total_tempo_tex_executada"},
        std::pair{"Registros de Espera", "registros_espera"},
        std::pair{"Número de Desvios", "numero_desvios"},
        std::pair{"Atividade Especial", "atividade_especial"},
        std::pair{"Data de reprogramação", "data_reprogramacao"},
        std::pair{"Setor Emissor Relacionado (2)", "setor_emissor_relacionado_2"},
        std::pair{"SN", "sn"},
    };

    REQUIRE(Catalog::sourceAliasCount() == 184);
    for (const auto& [header, expected] : samples) {
        INFO(header);
        REQUIRE(Catalog::canonicalColumnForHeader(std::string{header}) == expected);
    }
}

TEST_CASE("SSA spreadsheet mapper resolves repeated semantic header families by position") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "families.xlsx";
    table.rows = {
        {"Numero SSA",
         "Numero SSA",
         "Numero SSA",
         "Numero SSA",
         "Descricao",
         "Data Cadastro",
         "SN",
         "SN",
         "SN",
         "Setor Emissor",
         "Setor Emissor",
         "Setor Emissor",
         "Situacao",
         "Situacao",
         "Situacao",
         "Desde",
         "Desde",
         "Desde",
         "Ate",
         "Ate",
         "Ate"},
        {"202600001", "202600002", "202600003", "202600004", "Familias", "2026-07-14", "RET",
         "INS",       "EXT",       "EM0",       "EM1",       "EM2",      "APV",        "STE",
         "SCA",       "D0",        "D1",        "D2",        "A0",       "A1",         "A2"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    const auto& imported = batch.rows.front();
    REQUIRE(imported.at("numero_ssa") == "202600001");
    REQUIRE(imported.at("numero_ssa_relacionada_1") == "202600002");
    REQUIRE(imported.at("numero_ssa_relacionada_2") == "202600003");
    REQUIRE(imported.at("numero_ssa_relacionada_3") == "202600004");
    REQUIRE(imported.at("sn_retirado") == "RET");
    REQUIRE(imported.at("sn_instalado") == "INS");
    REQUIRE(imported.at("sn_extra") == "EXT");
    REQUIRE(imported.at("setor_emissor") == "EM0");
    REQUIRE(imported.at("setor_emissor_relacionado_1") == "EM1");
    REQUIRE(imported.at("setor_emissor_relacionado_2") == "EM2");
    REQUIRE(imported.at("situacao") == "APV");
    REQUIRE(imported.at("situacao_relacionada_1") == "STE");
    REQUIRE(imported.at("situacao_relacionada_2") == "SCA");
    REQUIRE(imported.at("desde") == "D0");
    REQUIRE(imported.at("desde_1") == "D1");
    REQUIRE(imported.at("desde_2") == "D2");
    REQUIRE(imported.at("ate") == "A0");
    REQUIRE(imported.at("ate_1") == "A1");
    REQUIRE(imported.at("ate_2") == "A2");
}

TEST_CASE("SSA spreadsheet mapper exposes the validated header for continuation chunks") {
    ssa::infra::importing::SpreadsheetTable first;
    first.sourcePath = "chunks.xlsx";
    first.rows = {{"Report cover"},
                  {"Numero SSA", "Descricao", "Data Cadastro"},
                  {"202600001", "First", "2026-07-14"}};

    const auto firstBatch = ssa::infra::importing::SsaSpreadsheetMapper::map(first);
    REQUIRE(firstBatch.headerRow ==
            std::vector<std::string>{"Numero SSA", "Descricao", "Data Cadastro"});

    ssa::infra::importing::SpreadsheetTable continuation;
    continuation.sourcePath = first.sourcePath;
    continuation.rows = {firstBatch.headerRow, {"202600002", "Second", "2026-07-14"}};
    const auto nextBatch = ssa::infra::importing::SsaSpreadsheetMapper::map(continuation);

    REQUIRE(nextBatch.rows.size() == 1);
    REQUIRE(nextBatch.rows.front().at("numero_ssa") == "202600002");
}

TEST_CASE("SSA spreadsheet mapper rejects an overflowing repeated header family") {
    ssa::infra::importing::SpreadsheetTable table;
    table.rows = {
        {"Numero SSA", "Descricao", "Data Cadastro", "SN", "SN", "SN", "SN"},
        {"202600001", "Ambigua", "2026-07-14", "A", "B", "C", "D"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::AmbiguousHeaders);
    REQUIRE(batch.rows.empty());
}

TEST_CASE("SSA spreadsheet mapper rejects duplicate non-positional header aliases") {
    ssa::infra::importing::SpreadsheetTable table;
    table.rows = {
        {"Numero SSA", "Nº SSA", "Descricao", "Descricao da SSA", "Data Cadastro"},
        {"", "202600010", "", "Valor complementar", "2026-07-14"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::AmbiguousHeaders);
    REQUIRE(batch.rows.empty());
}

TEST_CASE("SSA spreadsheet mapper rejects explicit and positional destination collisions") {
    ssa::infra::importing::SpreadsheetTable table;
    table.rows = {
        {"Numero SSA", "Número da SSA Relacionada", "Numero SSA", "Descricao", "Data Cadastro"},
        {"202600001", "202600002", "202600003", "Colisao", "2026-07-14"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::AmbiguousHeaders);
    REQUIRE(batch.rows.empty());
}
