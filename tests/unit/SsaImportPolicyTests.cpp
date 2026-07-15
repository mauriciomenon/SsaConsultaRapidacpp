#include "domain/ExecutionHistoryPolicy.h"
#include "domain/SsaImportPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

TEST_CASE("SSA import rejects invalid calendar dates even for exempt states") {
    using Values = ssa::domain::SsaImportPolicy::Values;

    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600001"},
                                                                  {"descricao_ssa", "Invalid date"},
                                                                  {"data_cadastro", "2026-13-99"},
                                                                  {"situacao", "ASE"}}));
    REQUIRE_FALSE(
        ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600002"},
                                                        {"descricao_ssa", "Exempt without date"},
                                                        {"situacao", "ASE"}}));
    REQUIRE(ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600003"},
                                                            {"descricao_ssa", "Exempt with week"},
                                                            {"situacao", "ASE"},
                                                            {"semana_cadastro", "202631"}}));
    REQUIRE(ssa::domain::SsaImportPolicy::isValidRow(
        Values{{"numero_ssa", "202600004"},
               {"descricao_ssa", "Exempt with descriptive status"},
               {"situacao", "ASE - aguardando execucao"},
               {"semana_cadastro", "202631"}}));
}

TEST_CASE("SSA import merge fails closed when neither snapshot is valid") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{
        {"numero_ssa", "202600010"}, {"descricao_ssa", "Existing"}, {"situacao", "APV"}};
    const Values incoming{
        {"numero_ssa", "202600010"}, {"descricao_ssa", "Incoming"}, {"situacao", "AAD"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values == existing);
    REQUIRE_FALSE(result.changed);
}

TEST_CASE("SSA import equal snapshot preserves state and completes empty fields") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600011"},
                          {"descricao_ssa", "Existing"},
                          {"situacao", "APV"},
                          {"setor_executor", ""},
                          {"data_planilha", "2026-05-01"}};
    const Values incoming{{"numero_ssa", "202600011"},
                          {"descricao_ssa", "Replacement"},
                          {"situacao", "AAD"},
                          {"setor_executor", "MEL1"},
                          {"prazo_limite", "2026-05-30"},
                          {"data_planilha", "2026-05-01"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values.at("situacao") == "APV");
    REQUIRE(result.values.at("descricao_ssa") == "Replacement");
    REQUIRE(result.values.at("setor_executor") == "MEL1");
    REQUIRE(result.values.at("prazo_limite") == "2026-05-30");
    REQUIRE(result.changed);
}

TEST_CASE("SSA import uses source file date only as the last snapshot fallback") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600012"},
                          {"descricao_ssa", "Newer"},
                          {"arquivo_origem", "ssa_02-05-2026_0100PM.xlsx"}};
    const Values incoming{{"numero_ssa", "202600012"},
                          {"descricao_ssa", "Older"},
                          {"arquivo_origem", "ssa_01-05-2026_0100PM.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values == existing);
    REQUIRE_FALSE(result.changed);
}

TEST_CASE("SSA import equal snapshot metadata is independent of file order") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values first{{"numero_ssa", "202600016"},
                       {"descricao_ssa", "Same"},
                       {"situacao", "APV"},
                       {"arquivo_origem", "A_SSA_14-07-2026_0145PM.xlsx"},
                       {"data_arquivo_origem", "2026-07-14 13:45:00"}};
    const Values second{{"numero_ssa", "202600016"},
                        {"descricao_ssa", "Same"},
                        {"situacao", "APV"},
                        {"arquivo_origem", "B_SSA_14-07-2026_0145PM.xlsx"},
                        {"data_arquivo_origem", "2026-07-14 13:45:00"}};

    const auto firstThenSecond = ssa::domain::SsaImportPolicy::merge(first, second);
    const auto secondThenFirst = ssa::domain::SsaImportPolicy::merge(second, first);

    REQUIRE(firstThenSecond.values == secondThenFirst.values);
    REQUIRE(firstThenSecond.values.at("arquivo_origem") == "A_SSA_14-07-2026_0145PM.xlsx");
}

TEST_CASE("SSA import compares Python source timestamps within the same day") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600013"},
                          {"descricao_ssa", "Earlier"},
                          {"situacao", "APV"},
                          {"data_arquivo_origem", "2026-07-13 13:30:00"}};
    const Values incoming{{"numero_ssa", "202600013"},
                          {"descricao_ssa", "Later"},
                          {"situacao", "STE"},
                          {"arquivo_origem", "SSA_13-07-2026 01:45 PM.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("situacao") == "STE");
    REQUIRE(result.values.at("descricao_ssa") == "Later");
}

TEST_CASE("SSA import uses filename timestamp before filesystem timestamps") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600019"},
                          {"descricao_ssa", "Newer filename"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSA_14-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-01"},
                          {"data_arquivo_origem", "2026-07-20 10:00:00"}};
    const Values incoming{{"numero_ssa", "202600019"},
                          {"descricao_ssa", "Older filename"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSA_13-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-30"},
                          {"data_arquivo_origem", "2026-07-31 10:00:00"},
                          {"data_criacao_arquivo", "2026-07-29 10:00:00"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.values == existing);
}

TEST_CASE("SSA import gives executed sources precedence on equal snapshots") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600021"},
                          {"descricao_ssa", "General snapshot"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSA_14-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-14"}};
    const Values incoming{{"numero_ssa", "202600021"},
                          {"descricao_ssa", "Executed snapshot"},
                          {"situacao", "STE"},
                          {"arquivo_origem", "SSAs executadas_14-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-14"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("descricao_ssa") == "Executed snapshot");
    REQUIRE(result.values.at("situacao") == "STE");
}

TEST_CASE("SSA import parses date-only source filenames") {
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp(
                "SSAs executadas_14-07-2026.xlsx") == "2026-07-14 00:00:00");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp("SSA_2026-07-15.xlsx") ==
            "2026-07-15 00:00:00");
}

TEST_CASE(
    "SSA import treats approved cancel and final execution as terminal but SCS as transient") {
    REQUIRE(ssa::domain::SsaImportPolicy::isTerminalStatus("STE - finalizada"));
    REQUIRE(ssa::domain::SsaImportPolicy::isTerminalStatus("SCA"));
    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isTerminalStatus("SCS"));
}

TEST_CASE("SSA import accepts final execution over a same snapshot transient state") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600020"},
                          {"descricao_ssa", "Execution"},
                          {"situacao", "APV"},
                          {"data_planilha", "2026-07-14"}};
    const Values incoming{{"numero_ssa", "202600020"},
                          {"descricao_ssa", "Execution"},
                          {"situacao", "STE"},
                          {"data_planilha", "2026-07-14"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("situacao") == "STE");
    REQUIRE_FALSE(result.conflict);
}

TEST_CASE("SSA import always promotes a transient state to an older terminal snapshot") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600024"},
                          {"descricao_ssa", "Current description"},
                          {"situacao", "APV"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSA_15-07-2026.xlsx"}};
    const Values incoming{{"numero_ssa", "202600024"},
                          {"descricao_ssa", "Older execution"},
                          {"situacao", "STE"},
                          {"data_planilha", "2026-07-14"},
                          {"arquivo_origem", "SSAs executadas_14-07-2026.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE_FALSE(result.conflict);
    REQUIRE(result.values.at("situacao") == "STE");
    REQUIRE(result.values.at("descricao_ssa") == "Current description");
}

TEST_CASE("SSA import older terminal promotion preserves newer metadata and indicators") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600025"},
                          {"descricao_ssa", "Current description"},
                          {"situacao", "APV"},
                          {"prazo_limite", "2026-07-30"},
                          {"tempo_excedido", "02:00"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSA_15-07-2026.xlsx"},
                          {"data_arquivo_origem", "2026-07-15 10:00:00"}};
    const Values incoming{{"numero_ssa", "202600025"},
                          {"descricao_ssa", "Older execution"},
                          {"situacao", "STE"},
                          {"prazo_limite", "2026-07-20"},
                          {"tempo_excedido", "01:00"},
                          {"data_planilha", "2026-07-14"},
                          {"arquivo_origem", "SSA_14-07-2026.xlsx"},
                          {"data_arquivo_origem", "2026-07-14 10:00:00"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values.at("situacao") == "STE");
    REQUIRE(result.values.at("prazo_limite") == "2026-07-30");
    REQUIRE(result.values.at("tempo_excedido") == "02:00");
    REQUIRE(result.values.at("data_planilha") == "2026-07-15");
    REQUIRE(result.values.at("arquivo_origem") == "SSA_15-07-2026.xlsx");
    REQUIRE(result.values.at("data_arquivo_origem") == "2026-07-15 10:00:00");
}

TEST_CASE("SSA import metadata does not make an equal snapshot richer") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600026"},
                          {"descricao_ssa", "Current description"},
                          {"situacao", "APV"},
                          {"data_planilha", "2026-07-15"}};
    const Values incoming{{"numero_ssa", "202600026"},
                          {"descricao_ssa", "Conflicting description"},
                          {"situacao", "APV"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSA_15-07-2026.xlsx"},
                          {"data_criacao_arquivo", "2026-07-15 11:00:00"},
                          {"data_arquivo_origem", "2026-07-15 12:00:00"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values.at("descricao_ssa") == "Current description");
    REQUIRE(result.conflict);
}

TEST_CASE("SSA import chooses the richer row on an equal snapshot") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values sparse{{"numero_ssa", "202600021"},
                        {"descricao_ssa", "Sparse"},
                        {"situacao", "APV"},
                        {"data_planilha", "2026-07-14"}};
    const Values rich{{"numero_ssa", "202600021"},
                      {"descricao_ssa", "Rich"},
                      {"situacao", "APV"},
                      {"setor_executor", "MEL1"},
                      {"descricao_execucao", "Parcial 1"},
                      {"tempo_excedido", "02:00"},
                      {"data_planilha", "2026-07-14"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(sparse, rich);

    REQUIRE(result.changed);
    REQUIRE_FALSE(result.conflict);
    REQUIRE(result.values.at("descricao_ssa") == "Rich");
    REQUIRE(result.values.at("setor_executor") == "MEL1");
}

TEST_CASE("SSA import preserves a rich field when a newer snapshot is sparse") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600022"}, {"descricao_ssa", "Description"},
                          {"situacao", "APV"},         {"descricao_execucao", "Parcial 1"},
                          {"tempo_excedido", "02:00"}, {"data_planilha", "2026-07-14"}};
    const Values incoming{{"numero_ssa", "202600022"},
                          {"descricao_ssa", "Description"},
                          {"situacao", "APV"},
                          {"tempo_excedido", "03:00"},
                          {"data_planilha", "2026-07-15"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("descricao_execucao") == "Parcial 1");
    REQUIRE(result.values.at("tempo_excedido") == "03:00");
}

TEST_CASE("SSA import classifies source profiles without changing the schema") {
    using Profile = ssa::domain::SsaImportPolicy::SourceProfile;
    REQUIRE(ssa::domain::SsaImportPolicy::classifySourceProfile("SSAs executadas.xlsx") ==
            Profile::Executadas);
    REQUIRE(ssa::domain::SsaImportPolicy::classifySourceProfile("relatorio de derivadas.xlsx") ==
            Profile::DerivadasRelacionadas);
    REQUIRE(ssa::domain::SsaImportPolicy::classifySourceProfile("SSA com desvio.xlsx") ==
            Profile::Desvios);
    REQUIRE(ssa::domain::SsaImportPolicy::classifySourceProfile("planejamento.xlsx") ==
            Profile::Geral);
}

TEST_CASE("SSA execution history keeps current overwrite behavior without numbered columns") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"descricao_execucao", "Parcial antiga"}};
    const std::vector<std::string> columns{"numero_ssa", "descricao_execucao"};

    REQUIRE(ssa::domain::ExecutionHistoryPolicy::targetColumn(existing, columns) ==
            "descricao_execucao");
}

TEST_CASE("SSA execution history selects the next available numbered column") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"descricao_execucao", "Parcial 1"},
                          {"descricao_execucao_2", "Parcial 2"}};
    const std::vector<std::string> columns{"descricao_execucao", "descricao_execucao_2",
                                           "descricao_execucao_3"};

    REQUIRE(ssa::domain::ExecutionHistoryPolicy::targetColumn(existing, columns) ==
            "descricao_execucao_3");
}

TEST_CASE("SSA import accepts the real cadastro timestamp format") {
    using Values = ssa::domain::SsaImportPolicy::Values;

    REQUIRE(
        ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600014"},
                                                        {"descricao_ssa", "Real timestamp"},
                                                        {"situacao", "APV"},
                                                        {"data_cadastro", "21/10/2025 11:10:36"}}));
}

TEST_CASE("SSA import normalizes a day first date without time") {
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("13-07-2026") ==
            "2026-07-13 00:00:00");
}

TEST_CASE("SSA import accepts an unambiguous US date for English spreadsheets") {
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("07/14/2025") ==
            "2025-07-14 00:00:00");
}

TEST_CASE("SSA import rejects invalid execution date fields before merge") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values row{{"numero_ssa", "202600027"},
                     {"descricao_ssa", "Invalid execution date"},
                     {"situacao", "APV"},
                     {"data_cadastro", "2026-07-15"},
                     {"data_programacao", "not-a-date"}};

    REQUIRE(ssa::domain::SsaImportPolicy::validateRow(row) ==
            ssa::domain::SsaImportPolicy::RowValidationIssue::InvalidDate);
}

TEST_CASE("SSA import field timestamps reject surrounding text") {
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("21/10/2025 11:10:36") ==
            "2025-10-21 11:10:36");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("2025-10-21T11:10:36") ==
            "2025-10-21 11:10:36");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("prefix 21/10/2025 11:10:36")
                .empty());
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("21/10/2025 11:10:36 suffix")
                .empty());
}

TEST_CASE("SSA import rejects cadastro timestamps with surrounding text") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const auto row = [](const std::string& timestamp) {
        return Values{{"numero_ssa", "202600015"},
                      {"descricao_ssa", "Strict date"},
                      {"situacao", "APV"},
                      {"data_cadastro", timestamp}};
    };

    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isValidRow(row("prefix 21/10/2025 11:10:36")));
    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isValidRow(row("21/10/2025 11:10:36 suffix")));
    REQUIRE(ssa::domain::SsaImportPolicy::isValidRow(row("2025-10-21T11:10:36")));
}

TEST_CASE("terminal SSA advances snapshot metadata while accepting newer indicators") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values terminal{{"numero_ssa", "202600017"},
                          {"descricao_ssa", "Terminal"},
                          {"situacao", "STE"},
                          {"prazo_limite", "2026-07-20"},
                          {"data_planilha", "2026-07-01"},
                          {"arquivo_origem", "SSA_01-07-2026.xlsx"}};
    const Values newest{{"numero_ssa", "202600017"},
                        {"descricao_ssa", "Must not replace terminal data"},
                        {"situacao", "APV"},
                        {"prazo_limite", "2026-07-30"},
                        {"data_planilha", "2026-07-03"},
                        {"arquivo_origem", "SSA_03-07-2026.xlsx"}};
    const Values middle{{"numero_ssa", "202600017"},
                        {"descricao_ssa", "Older than accepted snapshot"},
                        {"situacao", "APV"},
                        {"prazo_limite", "2026-07-25"},
                        {"data_planilha", "2026-07-02"},
                        {"arquivo_origem", "SSA_02-07-2026.xlsx"}};

    const auto advanced = ssa::domain::SsaImportPolicy::merge(terminal, newest);
    const auto ignored = ssa::domain::SsaImportPolicy::merge(advanced.values, middle);

    REQUIRE(advanced.changed);
    REQUIRE(advanced.values.at("situacao") == "STE");
    REQUIRE(advanced.values.at("descricao_ssa") == "Terminal");
    REQUIRE(advanced.values.at("prazo_limite") == "2026-07-30");
    REQUIRE(advanced.values.at("data_planilha") == "2026-07-03");
    REQUIRE(advanced.values.at("arquivo_origem") == "SSA_03-07-2026.xlsx");
    REQUIRE_FALSE(ignored.changed);
    REQUIRE(ignored.values == advanced.values);
}

TEST_CASE("terminal SSA preserves newer execution planning and description fields") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600018"},
                          {"situacao", "STE"},
                          {"descricao_ssa", "Finalizada"},
                          {"data_planilha", "2026-07-14"},
                          {"arquivo_origem", "SSAs executadas_14-07-2026.xlsx"}};
    const Values incoming{{"numero_ssa", "202600018"},
                          {"situacao", "STE"},
                          {"descricao_ssa", "Finalizada"},
                          {"semana_programada", "202630"},
                          {"responsavel_programacao", "Equipe A"},
                          {"responsavel_execucao", "Equipe B"},
                          {"descricao_execucao", "Execucao parcial 2"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSAs executadas_15-07-2026.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values.at("semana_programada") == "202630");
    REQUIRE(result.values.at("responsavel_programacao") == "Equipe A");
    REQUIRE(result.values.at("responsavel_execucao") == "Equipe B");
    REQUIRE(result.values.at("descricao_execucao") == "Execucao parcial 2");
}

TEST_CASE("terminal SSA never changes to another terminal state") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600023"},
                          {"descricao_ssa", "Approved cancellation"},
                          {"situacao", "SCA"},
                          {"data_planilha", "2026-07-01"}};
    const Values incoming{{"numero_ssa", "202600023"},
                          {"descricao_ssa", "Final execution"},
                          {"situacao", "STE"},
                          {"data_planilha", "2026-07-01"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.values == existing);
}

TEST_CASE("equal SSA snapshot enriches allowed indicators without reporting conflict") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600018"},
                          {"descricao_ssa", "Same business snapshot"},
                          {"situacao", "STE"},
                          {"prazo_limite", "2026-07-20"},
                          {"data_planilha", "2026-07-03"}};
    const Values incoming{{"numero_ssa", "202600018"},
                          {"descricao_ssa", "Same business snapshot"},
                          {"situacao", "STE"},
                          {"prazo_limite", "2026-07-30"},
                          {"data_planilha", "2026-07-03"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE_FALSE(result.conflict);
    REQUIRE(result.values.at("prazo_limite") == "2026-07-30");
}
