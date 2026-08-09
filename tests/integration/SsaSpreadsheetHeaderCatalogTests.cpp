#include "domain/ColumnCatalog.h"
#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "infra/import/SsaSpreadsheetMapper.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>
#include <utility>

TEST_CASE("SSA spreadsheet header catalog preserves the Python import contract") {
    using Catalog = ssa::infra::importing::SsaSpreadsheetHeaderCatalog;
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 13> samples{
        std::pair{"Nº SSA*", "numero_ssa"},
        std::pair{"process_status", "situacao"},
        std::pair{"Fecha de Emision", "data_cadastro"},
        std::pair{"Prazo Limite", "status_execucao_prazo"},
        std::pair{"Data Limite", "data_limite"},
        std::pair{"Tempo TPE Plan.", "total_tempo_tpe_planejado"},
        std::pair{"Total Tempo TEX Executada", "total_tempo_tex_executada"},
        std::pair{"Registros de Espera", "registros_espera"},
        std::pair{"Número de Desvios", "numero_desvios"},
        std::pair{"Atividade Especial", "atividade_especial"},
        std::pair{"Data de reprogramação", "data_reprogramacao"},
        std::pair{"Setor Emissor Relacionado (2)", "setor_emissor_relacionado_2"},
        std::pair{"SN", "sn"},
    };

    REQUIRE(Catalog::sourceLabelCount() == 184);
    for (const auto& [header, expected] : samples) {
        INFO(header);
        REQUIRE(Catalog::canonicalColumnForHeader(std::string{header}) == expected);
    }

    const auto* deadlineStatus = ssa::domain::ColumnCatalog::find("status_execucao_prazo");
    const auto* deadlineDate = ssa::domain::ColumnCatalog::find("data_limite");
    REQUIRE(deadlineStatus != nullptr);
    REQUIRE(deadlineStatus->type == ssa::domain::ColumnType::Text);
    REQUIRE(deadlineDate != nullptr);
    REQUIRE(deadlineDate->type == ssa::domain::ColumnType::DateText);
}

TEST_CASE("SSA spreadsheet aliases converge to stable ASCII schema keys") {
    using Catalog = ssa::infra::importing::SsaSpreadsheetHeaderCatalog;

    REQUIRE(Catalog::canonicalColumnForHeader("Descricao da SSA") == "descricao_ssa");
    REQUIRE(Catalog::canonicalColumnForHeader("Description") == "descricao_ssa");
    REQUIRE(Catalog::canonicalColumnForHeader("Descripcion de la SSA") == "descricao_ssa");
    REQUIRE(Catalog::canonicalColumnForHeader("Numero de Desvios") == "numero_desvios");
    REQUIRE_FALSE(Catalog::canonicalColumnForHeader("unknown external header"));
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

TEST_CASE("SSA spreadsheet mapper preserves a malformed related number for validation") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "malformed-related-number.xlsx";
    table.rows = {{"Numero SSA", "Numero SSA", "Descricao", "Data Cadastro"},
                  {"202600001", "INVALID-REF", "Malformed relation", "2026-07-14"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    REQUIRE(batch.rows.front().at("numero_ssa_relacionada_1") == "INVALID-REF");
}

TEST_CASE("SSA spreadsheet mapper rejects a malformed primary number with empty description") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "malformed-primary-number.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro"},
                  {"INVALID-PRIMARY", "", "2026-07-14"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.empty());
    REQUIRE(batch.invalidRows == 1);
    REQUIRE(batch.invalidNumberRows == 1);
}

TEST_CASE("SSA spreadsheet mapper identifies a derivation relation report as auxiliary") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs Derivadas e Relacionadas.xlsx";
    table.rows = {
        {"Numero da SSA", "Localizacao", "Situacao", "Numero da SSA", "Situacao", "Relacao",
         "Numero da SSA", "Situacao"},
        {"202300227", "T075Q002", "STE", "202300204", "STE", "Derivada da", "202223156", "STE"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::HeaderNotRecognized);
    REQUIRE(batch.rows.empty());
}

TEST_CASE("SSA spreadsheet mapper identifies a compact related-SSA report as auxiliary") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs Relacionadas.xlsx";
    table.rows = {
        {"Categoria", "Numero da SSA", "Localizacao", "Setor Emissor", "Setor Executor",
         "Situacao"},
        {"Consequencia", "202604849", "T075Q002", "IEE3", "MEL4", "SEE"},
    };

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::HeaderNotRecognized);
    REQUIRE(batch.rows.empty());
}

TEST_CASE("SSA spreadsheet mapper preserves raw SAM API reports for the dedicated flow") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "pai_sam_api.xlsx";
    table.rows = {{"ssa_number", "localization", "description", "issue_datetime",
                   "emission_datetime", "emitter_sector", "executor_sector", "year_week",
                   "situation_desc", "process_status", "detail_present"},
                  {"202600233", "M075A006", "Description", "2026-01-07T13:56:00Z", "", "MEL4",
                   "IEE3", "202602", "", "", "0"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::HeaderNotRecognized);
    REQUIRE(batch.rows.empty());
}

TEST_CASE("SSA spreadsheet mapper extracts the integer from a real deviation label") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs com Desvio.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Desvio"},
                  {"202600001", "Deviation", "2026-07-14", "Desvio #2"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    REQUIRE(batch.rows.front().at("numero_desvios") == "2");
}

TEST_CASE("SSA spreadsheet mapper extracts the integer from a reschedule label") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAsRescheduled.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Reprogramacoes"},
                  {"202600001", "Rescheduled", "2026-07-14", "Reschedule #2"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    REQUIRE(batch.rows.front().at("num_reprogramacoes") == "2");
}

TEST_CASE("SSA spreadsheet mapper omits a nonnumeric final reschedule label") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAsRescheduled.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Reprogramacoes"},
                  {"202600001", "Rescheduled", "2026-07-14", "Final reschedule"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    REQUIRE_FALSE(batch.rows.front().contains("num_reprogramacoes"));
}

TEST_CASE("SSA spreadsheet mapper ignores known incomplete legacy summary rows") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "Todas as SSAs - 14-07-2022.xlsx";
    table.rows = {
        {"Numero da SSA", "Situacao", "Descricao da SSA", "Emitida Em", "Semana de Cadastro"},
        {"202213482", "SCC", "", "", "202223"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.empty());
    REQUIRE(batch.invalidRows == 0);
    REQUIRE(batch.skippedRows == 1);
}

TEST_CASE("SSA spreadsheet mapper still rejects incomplete rows from normal reports") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs executadas.xlsx";
    table.rows = {
        {"Numero da SSA", "Situacao", "Descricao da SSA", "Emitida Em", "Semana de Cadastro"},
        {"202213482", "SCC", "", "", "202223"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.invalidRows == 1);
    REQUIRE(batch.invalidDescriptionRows == 1);
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

TEST_CASE("SSA spreadsheet mapper consumes an external header without shifting chunk rows") {
    ssa::infra::importing::SpreadsheetTable continuation;
    continuation.sourcePath = "external-header.xlsx";
    continuation.headerRow = {"Numero SSA", "Descricao", "Data Cadastro"};
    continuation.rows = {{"202600002", "Second", "2026-07-14"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(continuation);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    REQUIRE(batch.rows.front().at("numero_ssa") == "202600002");
    REQUIRE(batch.headerRow == continuation.headerRow);
}

TEST_CASE("SSA status filter contracts live in the domain catalog") {
    const auto shortcuts = ssa::domain::ColumnCatalog::statusShortcutCodes();

    REQUIRE(shortcuts.size() == 26);
    REQUIRE(shortcuts.front() == "AAD");
    REQUIRE(shortcuts.back() == "STE");
    REQUIRE(ssa::domain::ColumnCatalog::downloadableStatusFilterExpression() ==
            "!SAD,!SCA,!SES,!STE");
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
