#include "domain/SsaImportPolicy.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SSA import rejects invalid calendar dates even for exempt states") {
    using Values = ssa::domain::SsaImportPolicy::Values;

    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600001"},
                                                                  {"descricao_ssa", "Invalid date"},
                                                                  {"data_cadastro", "2026-13-99"},
                                                                  {"situacao", "ASE"}}));
    REQUIRE(
        ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600002"},
                                                        {"descricao_ssa", "Exempt without date"},
                                                        {"situacao", "ASE"}}));
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
    REQUIRE(result.values.at("descricao_ssa") == "Existing");
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
