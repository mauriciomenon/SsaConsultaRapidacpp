#include "domain/ColumnCatalog.h"
#include "infra/import/SsaSpreadsheetHeaderCatalog.h"
#include "infra/import/SsaSpreadsheetMapper.h"
#include "ports/OperationError.h"

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

TEST_CASE("SSA spreadsheet mapper stores time window bounds as ISO timestamps") {
    // Python stores desde/ate as "YYYY-MM-DD HH:MM:SS"; raw dd/mm/yyyy text mixed both
    // formats in one column and made equal instants conflict.
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs Pendentes com Execucao Parcial_20-10-2026_1127AM.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Tempo Disponivel", "Desde", "Ate",
                   "Tempo Excedido", "Desde", "Ate", "Tempo Total", "Desde", "Ate"},
                  {"202600001", "Window", "2026-07-14", "1d", "06/02/2026 14:55:07",
                   "07/02/2026 08:00:00", "2d", "2026-02-08T09:10:11", "sem data", "3d",
                   "09/02/2026 10:00:00", "2026-02-10"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 1);
    const auto& row = batch.rows.front();
    REQUIRE(row.at("desde") == "2026-02-06 14:55:07");
    REQUIRE(row.at("ate") == "2026-02-07 08:00:00");
    REQUIRE(row.at("desde_1") == "2026-02-08 09:10:11");
    REQUIRE_FALSE(row.contains("ate_1"));
    REQUIRE(row.at("desde_2") == "2026-02-09 10:00:00");
    REQUIRE(row.at("ate_2") == "2026-02-10 00:00:00");
    for (const auto* key : {"desde", "desde_1", "desde_2", "ate", "ate_1", "ate_2"}) {
        INFO(key);
        REQUIRE(ssa::domain::ColumnCatalog::find(key)->type ==
                ssa::domain::ColumnType::DateTimeText);
    }
}

TEST_CASE("SSA spreadsheet mapper normalizes all upsert date keys and drops invalid dates") {
    using namespace ssa::infra::importing;
    for (const auto* key : {"data_cadastro", "prazo_limite", "data_limite", "desde", "desde_1",
                            "desde_2", "ate", "ate_1", "ate_2", "data_inicio_programada",
                            "data_programacao", "data_inicio_reprogramada", "data_reprogramacao",
                            "instalacao_estimada", "executado", "concluido"}) {
        INFO(key);
        SpreadsheetTable table;
        table.sourcePath = "dates.xlsx";
        table.headerRow = {"numero_ssa", "descricao_ssa", "data_cadastro", "situacao",
                           "semana_cadastro"};
        table.rows = {{"202600001", "Dates", "2025-07-14", "ASE", "202529"}};
        const std::size_t dateIndex = std::string_view{key} == "data_cadastro" ? 2 : 5;
        if (dateIndex == 5) {
            table.headerRow.push_back(key);
            table.rows.front().emplace_back();
        }
        for (const auto& [input, expected] : std::vector<std::pair<std::string, std::string>>{
                 {"09/10/2025", "2025-10-09 00:00:00"},
                 {"2025-07-14T15:43:12", "2025-07-14 15:43:12"},
                 {"2025-07-14", "2025-07-14 00:00:00"},
                 {"11/02/2022 14:57:56", "2022-02-11 14:57:56"},
                 {"bad-date", ""},
                 {"45205.5", ""}}) {
            INFO(input);
            table.rows.front()[dateIndex] = input;
            const auto batch = SsaSpreadsheetMapper::map(table);
            REQUIRE(batch.rows.size() == 1);
            if (expected.empty()) {
                REQUIRE_FALSE(batch.rows.front().contains(key));
            } else {
                REQUIRE(batch.rows.front().at(key) == expected);
            }
        }
    }
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
        {"202600001",  "202600002",  "202600003", "202600004",  "Familias",   "2026-07-14",
         "RET",        "INS",        "EXT",       "EM0",        "EM1",        "EM2",
         "APV",        "STE",        "SCA",       "2026-07-10", "2026-07-11", "2026-07-12",
         "2026-07-20", "2026-07-21", "2026-07-22"},
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
    // Date columns: distinct valid dates prove the position, stored as date and time.
    REQUIRE(imported.at("desde") == "2026-07-10 00:00:00");
    REQUIRE(imported.at("desde_1") == "2026-07-11 00:00:00");
    REQUIRE(imported.at("desde_2") == "2026-07-12 00:00:00");
    REQUIRE(imported.at("ate") == "2026-07-20 00:00:00");
    REQUIRE(imported.at("ate_1") == "2026-07-21 00:00:00");
    REQUIRE(imported.at("ate_2") == "2026-07-22 00:00:00");
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

TEST_CASE("SSA spreadsheet mapper omits a deviation row label") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAs com Desvio.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Desvio"},
                  {"202600001", "Deviation", "2026-07-14", "Desvio #2"},
                  {"202600002", "Count", "2026-07-14", "3"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 2);
    REQUIRE_FALSE(batch.rows[0].contains("numero_desvios"));
    REQUIRE(batch.rows[1].at("numero_desvios") == "3");
}

TEST_CASE("SSA spreadsheet mapper omits a reschedule row label") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAsRescheduled.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Reprogramacoes"},
                  {"202600001", "Rescheduled", "2026-07-14", "Reschedule #2"},
                  {"202600002", "Reprogramada", "2026-07-14", "Reprograma\u00e7\u00e3o #1"},
                  {"202600003", "Count", "2026-07-14", "4"}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(batch.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(batch.rows.size() == 3);
    REQUIRE_FALSE(batch.rows[0].contains("num_reprogramacoes"));
    REQUIRE_FALSE(batch.rows[1].contains("num_reprogramacoes"));
    REQUIRE(batch.rows[2].at("num_reprogramacoes") == "4");
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

TEST_CASE("SSA spreadsheet mapper counts continuation rows as skipped and blank rows not") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "SSAsRescheduled.xlsx";
    table.rows = {{"Numero SSA", "Descricao", "Data Cadastro", "Reprogramacoes"},
                  {"202600001", "Rescheduled", "2026-07-14", "Reprogramacao #1"},
                  {"", "", "2026-07-21", "Reprogramacao #2"},
                  {"", "", "", "Reprogramacao #3"},
                  {"", "", "", ""}};

    const auto batch = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    // The label alone maps to no value (it is not a count), yet the row is not blank.
    REQUIRE(batch.rows.size() == 1);
    REQUIRE(batch.invalidRows == 0);
    REQUIRE(batch.skippedRows == 2);
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

TEST_CASE("SSA spreadsheet mapper coerces counts and backfills only a present primary column") {
    using namespace ssa::infra::importing;
    for (const auto& [primary, expected] :
         std::vector<std::pair<std::string, std::string>>{{"Reprogramacao #2", "7"},
                                                          {"0", "0"},
                                                          {"3.0", "3"},
                                                          {"-2", "-2"},
                                                          {"", "7"},
                                                          {"NaN", "7"}}) {
        INFO(primary);
        SpreadsheetTable table;
        table.headerRow = {"Numero SSA", "Descricao", "Data Cadastro", "num_reprogramacoes",
                           "total_de_reprogramacoes"};
        table.rows = {{"202600001", "Counts", "2026-07-14", primary, "7"}};
        const auto batch = SsaSpreadsheetMapper::map(table);
        REQUIRE(batch.rows.size() == 1);
        REQUIRE(batch.rows.front().at("num_reprogramacoes") == expected);
        REQUIRE(batch.rows.front().at("total_de_reprogramacoes") == "7");
    }
    SpreadsheetTable table;
    table.headerRow = {"Numero SSA", "Descricao", "Data Cadastro", "total_de_reprogramacoes"};
    table.rows = {{"202600001", "Counts", "2026-07-14", "7"}};
    const auto absent = SsaSpreadsheetMapper::map(table);
    REQUIRE(absent.rows.size() == 1);
    REQUIRE_FALSE(absent.rows.front().contains("num_reprogramacoes"));
    REQUIRE(absent.rows.front().at("total_de_reprogramacoes") == "7");

    // A short row omits the primary cell, but the header still contains the column.
    table.headerRow.push_back("num_reprogramacoes");
    const auto empty = SsaSpreadsheetMapper::map(table);
    REQUIRE(empty.rows.size() == 1);
    REQUIRE(empty.rows.front().at("num_reprogramacoes") == "7");
    table.rows.front()[3] = "0";
    const auto zero = SsaSpreadsheetMapper::map(table);
    REQUIRE(zero.rows.size() == 1);
    REQUIRE(zero.rows.front().at("num_reprogramacoes") == "0");
}

TEST_CASE("SSA spreadsheet mapper drops nonnumeric deviation text") {
    using namespace ssa::infra::importing;
    for (const auto* text : {"Desvio: 2", "sem desvio", "2,0", "Desvio #2"}) {
        INFO(text);
        SpreadsheetTable table;
        table.headerRow = {"Numero SSA", "Descricao", "Data Cadastro", "numero_desvios"};
        table.rows = {{"202600001", "Counts", "2026-07-14", text}};
        const auto batch = SsaSpreadsheetMapper::map(table);
        REQUIRE(batch.rows.size() == 1);
        REQUIRE_FALSE(batch.rows.front().contains("numero_desvios"));
    }
}

TEST_CASE("SSA spreadsheet mapper rejects the file for invalid integer counts") {
    using namespace ssa::infra::importing;
    for (const auto* key : {"num_reprogramacoes", "total_de_reprogramacoes", "numero_desvios"}) {
        for (const auto* text : {"2.5", "inf", "9223372036854775808"}) {
            INFO(key);
            INFO(text);
            SpreadsheetTable table;
            table.headerRow = {"Numero SSA",
                               "Descricao",
                               "Data Cadastro",
                               "num_reprogramacoes",
                               "total_de_reprogramacoes",
                               "numero_desvios"};
            table.rows = {{"202600001", "Valid", "2026-07-14", "3", "7", "0"},
                          {"202600002", "Invalid", "2026-07-14", "3", "7", "0"}};
            const auto column = key == std::string_view{"num_reprogramacoes"}        ? 3
                                : key == std::string_view{"total_de_reprogramacoes"} ? 4
                                                                                     : 5;
            table.rows.back()[column] = text;
            try {
                static_cast<void>(SsaSpreadsheetMapper::map(table));
                FAIL("Expected InvalidInteger to reject the entire file");
            } catch (const ssa::ports::OperationError& error) {
                REQUIRE(error.diagnostic().find("InvalidInteger") != std::string::npos);
            }
        }
    }
}
