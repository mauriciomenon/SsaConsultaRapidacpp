#include "domain/ExecutionHistoryPolicy.h"
#include "domain/SsaImportPolicy.h"
#include "domain/WhitespaceTrim.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("SSA import normalizes whole exceeded time expressions without carrying units") {
    using Policy = ssa::domain::SsaImportPolicy;
    REQUIRE(Policy::normalizeExceededTime("1a, 9m, 22d 5h : 47mi") == "P1Y9M22DT5H47M");
    REQUIRE(Policy::normalizeExceededTime("1m, 2mi") == "P1MT2M");
    REQUIRE(Policy::normalizeExceededTime("1h30m") == "PT1H30M");
    REQUIRE(Policy::normalizeExceededTime("25d 1h : 9mi") == "P25DT1H9M");
    REQUIRE(Policy::normalizeExceededTime("3m, 25d 21h : 20mi") == "P3M25DT21H20M");
    REQUIRE(Policy::normalizeExceededTime("2h 2h") == "PT4H");
    REQUIRE(Policy::normalizeExceededTime(" 60MI 24H 0D 2MO 1A ") == "P1Y2MT24H60M");
    REQUIRE(Policy::normalizeExceededTime("1m \t, 2m : , 3mi") == "P1MT5M");
    REQUIRE(Policy::normalizeExceededTime("1\xC2\xA0h\xE3\x80\x80"
                                          "30m") == "PT1H30M");
    REQUIRE(Policy::normalizeExceededTime("1m: ,2mi") == "PT3M");
    REQUIRE(Policy::normalizeExceededTime("1mi,2mo") == "P2MT1M");
}

TEST_CASE("SSA import preserves trimmed exceeded time when incomplete invalid or zero") {
    using Policy = ssa::domain::SsaImportPolicy;
    REQUIRE(Policy::normalizeExceededTime(" 1h 15minutes ") == "1h 15minutes");
    for (const auto* text : {"0h", "0a 0mo 0d 0h 0mi", "", ",:", "PT1H", "unknown", "1h!", "-1h",
                             "1.5h", "1h 30", "1h30mx", "m", "1h x 2h"}) {
        INFO(text);
        REQUIRE(Policy::normalizeExceededTime(std::string{" \t"} + text + " \n") == text);
    }
}

TEST_CASE("SSA import preserves exceeded time on numeric or accumulated overflow") {
    using Policy = ssa::domain::SsaImportPolicy;
    const auto maximum = std::to_string(std::numeric_limits<unsigned long long>::max());
    REQUIRE(Policy::normalizeExceededTime(maximum + "mi") == "PT" + maximum + "M");
    for (const auto& text :
         {maximum + "0h", maximum + "h 1h", maximum + "m, 1mo", maximum + "mi 1m"}) {
        INFO(text);
        REQUIRE(Policy::normalizeExceededTime(" " + text + " ") == text);
    }
}

TEST_CASE("SSA import removes null sentinels before validating imported values") {
    using Policy = ssa::domain::SsaImportPolicy;
    for (const auto* sentinel :
         {"", " \t", " \tNuLl ", " <NA> ", " N/A ", " - ", " None ", " NaN "}) {
        INFO(sentinel);
        Policy::Values values{{"numero_ssa", "202600001"},
                              {"setor_executor", sentinel},
                              {"derivada_de", sentinel},
                              {"num_reprogramacoes", sentinel}};
        REQUIRE(Policy::normalizeImportedRow(values) == Policy::ImportedRowIssue::None);
        REQUIRE(values.size() == 1);
        REQUIRE(values.at("numero_ssa") == "202600001");
    }
}

TEST_CASE("SSA import trims business values and preserves internal spaces and literal na") {
    using Policy = ssa::domain::SsaImportPolicy;
    Policy::Values values{{"numero_ssa", " 202600001 "},
                          {"setor_executor", "  Equipe A  "},
                          {"situacao", "\tA\r\n"},
                          {"descricao_ssa", " A  B "},
                          {"setor_emissor", "na"},
                          {"localizacao_codigo", "nat"},
                          {"arquivo_origem", "\xE2\x80\xA8source.xlsx\xE3\x80\x80"}};
    REQUIRE(Policy::normalizeImportedRow(values) == Policy::ImportedRowIssue::None);
    REQUIRE(values.at("numero_ssa") == "202600001");
    REQUIRE(values.at("setor_executor") == "Equipe A");
    REQUIRE(values.at("situacao") == "A");
    REQUIRE(values.at("descricao_ssa") == "A  B");
    REQUIRE(values.at("setor_emissor") == "na");
    REQUIRE(values.at("localizacao_codigo") == "nat");
    REQUIRE(values.at("arquivo_origem") == "source.xlsx");
    const auto normalized = values;
    REQUIRE(Policy::normalizeImportedRow(values) == Policy::ImportedRowIssue::None);
    REQUIRE(values == normalized);
}

TEST_CASE("SSA import rejects a null sentinel number") {
    using Policy = ssa::domain::SsaImportPolicy;
    Policy::Values values{{"numero_ssa", "null"}};
    REQUIRE(Policy::normalizeImportedRow(values) == Policy::ImportedRowIssue::InvalidNumber);
    REQUIRE_FALSE(values.contains("numero_ssa"));
}

TEST_CASE("whitespace trim handles text empty and whitespace-only values") {
    REQUIRE(ssa::domain::trimWhitespace("  SSA  ") == "SSA");
    REQUIRE(ssa::domain::trimWhitespace("") == "");
    REQUIRE(ssa::domain::trimWhitespace(" \t\r\n\f\v") == "");
}

TEST_CASE("whitespace trim removes what Python str.strip removes") {
    // U+0085, U+1680, U+2000..U+200A, U+2028, U+2029, U+202F, U+205F, U+3000 and \x1c..\x1f.
    REQUIRE(ssa::domain::trimWhitespace("\xE2\x80\xA8texto\xE3\x80\x80") == "texto");
    REQUIRE(ssa::domain::trimWhitespace("\xC2\x85\xE1\x9A\x80texto\xE2\x80\x8A\xE2\x80\xAF") ==
            "texto");
    REQUIRE(ssa::domain::trimWhitespace("\x1ctexto\x1f\xE2\x81\x9F\xE2\x80\xA9") == "texto");
    REQUIRE(ssa::domain::trimWhitespace("\xE2\x80\x80\xE2\x80\x85") == "");
    // Inner whitespace stays; U+200B (zero width space) is not whitespace for Python.
    REQUIRE(ssa::domain::trimWhitespace(" a\xE3\x80\x80"
                                        "b ") == "a\xE3\x80\x80"
                                                 "b");
    REQUIRE(ssa::domain::trimWhitespace("\xE2\x80\x8Btexto") == "\xE2\x80\x8Btexto");
    // A lone continuation-like byte is never treated as part of a whitespace sequence.
    REQUIRE(ssa::domain::trimWhitespace("texto\xA0") == "texto\xA0");
}

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
    REQUIRE_FALSE(
        ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600005"},
                                                        {"descricao_ssa", "Invalid ISO week 53"},
                                                        {"situacao", "ASE"},
                                                        {"semana_cadastro", "202153"}}));
    REQUIRE(ssa::domain::SsaImportPolicy::isValidRow(Values{{"numero_ssa", "202600006"},
                                                            {"descricao_ssa", "Valid ISO week 53"},
                                                            {"situacao", "ASE"},
                                                            {"semana_cadastro", "202053"}}));
    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isValidRow(
        Values{{"numero_ssa", "202600007"},
               {"descricao_ssa", "Dated row with invalid ISO week 53"},
               {"data_cadastro", "2021-01-01"},
               {"situacao", "APL"},
               {"semana_cadastro", "202153"}}));
}

TEST_CASE("SSA import normalization trims spaces and whitespace-only values") {
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeNumber(" 2026-00001.0 ") == "202600001");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeNumber("\t\r\n") == "");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeNumber("") == "");
}

TEST_CASE("SSA import integer counts coerce numeric text without semantic labels") {
    using Policy = ssa::domain::SsaImportPolicy;
    for (const auto* text : {"Desvio #1", "desvio # 2", "Desvio: 2", "Desvios: 3", "sem desvio",
                             "2,0", "Reprogramacao #2", "N/A", "NaN", "", "3x"}) {
        INFO(text);
        REQUIRE(Policy::normalizeIntegerCount(text) == std::optional<std::string>{""});
    }
    for (const auto& [text, expected] : std::vector<std::pair<std::string, std::string>>{
             {"3", "3"},
             {"3.0", "3"},
             {"-2", "-2"},
             {" +003.00 ", "3"},
             {"2e2", "200"},
             {"20e-1", "2"},
             {"-0.0", "0"},
             {".0", "0"},
             {"3.", "3"},
             {"9223372036854775807", "9223372036854775807"},
             {"9223372036854775807.0", "9223372036854775807"},
             {"-9223372036854775808.0", "-9223372036854775808"}}) {
        INFO(text);
        REQUIRE(Policy::normalizeIntegerCount(text) == std::optional<std::string>{expected});
    }
}

TEST_CASE("SSA import integer counts reject fractions infinity and int64 overflow") {
    using Policy = ssa::domain::SsaImportPolicy;
    for (const auto* text :
         {"2.5", "-1.5", "1e-1", "Inf", "-infinity", "+INF", "9223372036854775808",
          "-9223372036854775809", "9223372036854775808.0", "9223372036854775807.1",
          "1e999999999999999999999", "1e-999999999999999999999"}) {
        INFO(text);
        REQUIRE_FALSE(Policy::normalizeIntegerCount(text).has_value());
        for (const auto* key :
             {"num_reprogramacoes", "total_de_reprogramacoes", "numero_desvios"}) {
            INFO(key);
            Policy::Values row{{"numero_ssa", "202600001"}, {key, text}};
            REQUIRE(Policy::normalizeImportedRow(row) == Policy::ImportedRowIssue::InvalidInteger);
        }
    }
}

TEST_CASE("SSA import count backfill requires explicit header presence") {
    using Policy = ssa::domain::SsaImportPolicy;
    Policy::Values absent{{"total_de_reprogramacoes", "7"}};
    REQUIRE(Policy::normalizeImportedCounts(absent, false) == Policy::ImportedRowIssue::None);
    REQUIRE_FALSE(absent.contains("num_reprogramacoes"));
    REQUIRE(Policy::normalizeImportedCounts(absent, true) == Policy::ImportedRowIssue::None);
    REQUIRE(absent.at("num_reprogramacoes") == "7");

    Policy::Values zero{{"num_reprogramacoes", "0"}, {"total_de_reprogramacoes", "7"}};
    REQUIRE(Policy::normalizeImportedCounts(zero, true) == Policy::ImportedRowIssue::None);
    REQUIRE(zero.at("num_reprogramacoes") == "0");
    Policy::Values invalid{{"num_reprogramacoes", "3"}, {"total_de_reprogramacoes", "2.5"}};
    REQUIRE(Policy::normalizeImportedCounts(invalid, true) ==
            Policy::ImportedRowIssue::InvalidInteger);
}

TEST_CASE("SSA import equal snapshot preserves state and completes empty fields") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600011"},
                          {"descricao_ssa", "Existing"},
                          {"situacao", "APV"},
                          {"setor_executor", ""},
                          {"responsavel_execucao", "Equipe A"},
                          {"data_planilha", "2026-05-01"}};
    const Values incoming{{"numero_ssa", "202600011"},
                          {"descricao_ssa", "Existing"},
                          {"situacao", "APV"},
                          {"setor_executor", "MEL1"},
                          {"responsavel_execucao", ""},
                          {"prazo_limite", "2026-05-30"},
                          {"data_planilha", "2026-05-01"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.values.at("situacao") == "APV");
    REQUIRE(result.values.at("descricao_ssa") == "Existing");
    REQUIRE(result.values.at("setor_executor") == "MEL1");
    REQUIRE(result.values.at("responsavel_execucao") == "Equipe A");
    REQUIRE(result.values.at("prazo_limite") == "2026-05-30");
    REQUIRE(result.changed);
    REQUIRE_FALSE(result.conflict);
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

TEST_CASE("SSA import skips invalid snapshot metadata and uses the next valid timestamp") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600028"},
                          {"descricao_ssa", "Existing"},
                          {"situacao", "APV"},
                          {"data_planilha", "invalid"},
                          {"data_arquivo_origem", "2026-07-15 10:00:00"}};
    const Values incoming{{"numero_ssa", "202600028"},
                          {"descricao_ssa", "Newer"},
                          {"situacao", "APV"},
                          {"data_arquivo_origem", "2026-07-16 10:00:00"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("descricao_ssa") == "Newer");
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
    // Equal content: the larger canonical serialization wins, in either order.
    REQUIRE(firstThenSecond.values.at("arquivo_origem") == "B_SSA_14-07-2026_0145PM.xlsx");
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

TEST_CASE("SSA import sheet date decides before the filename timestamp") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    // Python tuple (data_planilha, file date): a newer sheet date wins over a newer filename.
    const Values existing{{"numero_ssa", "202600019"},
                          {"descricao_ssa", "Newer filename"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSA_14-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-01"},
                          {"data_arquivo_origem", "2026-07-20 10:00:00"}};
    const Values incoming{{"numero_ssa", "202600019"},
                          {"descricao_ssa", "Newer sheet date"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSA_13-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-30"},
                          {"data_arquivo_origem", "2026-07-31 10:00:00"},
                          {"data_criacao_arquivo", "2026-07-29 10:00:00"}};

    const auto newerSheet = ssa::domain::SsaImportPolicy::merge(existing, incoming);
    REQUIRE(newerSheet.changed);
    REQUIRE(newerSheet.values.at("descricao_ssa") == "Newer sheet date");

    // Equal sheet date: the filename date breaks the tie.
    Values sameSheet = incoming;
    sameSheet["data_planilha"] = "2026-07-01";
    const auto olderFilename = ssa::domain::SsaImportPolicy::merge(existing, sameSheet);
    REQUIRE_FALSE(olderFilename.changed);
    REQUIRE(olderFilename.values == existing);
}

TEST_CASE("SSA import equal snapshot with equal field count is decided by content, not order") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values general{{"numero_ssa", "202600021"},
                         {"descricao_ssa", "General snapshot"},
                         {"situacao", "APV"},
                         {"arquivo_origem", "SSA_14-07-2026_0100PM.xlsx"},
                         {"data_planilha", "2026-07-14"}};
    const Values executed{{"numero_ssa", "202600021"},
                          {"descricao_ssa", "Executed snapshot"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "SSAs executadas_14-07-2026_0100PM.xlsx"},
                          {"data_planilha", "2026-07-14"}};

    const auto forward = ssa::domain::SsaImportPolicy::merge(general, executed);
    const auto backward = ssa::domain::SsaImportPolicy::merge(executed, general);

    REQUIRE_FALSE(forward.conflict);
    REQUIRE_FALSE(backward.conflict);
    REQUIRE(forward.values == backward.values);
    REQUIRE(forward.values.at("descricao_ssa") == "Executed snapshot");
    REQUIRE(forward.changed);
    REQUIRE_FALSE(backward.changed);
    REQUIRE(backward.repeatDiscarded);
}

TEST_CASE("SSA import ignores date-only source filenames") {
    REQUIRE(
        ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp("SSAs executadas_14-07-2026.xlsx")
            .empty());
    REQUIRE(
        ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp("SSA_2026-07-15.xlsx").empty());
}

TEST_CASE("SSA import keeps the hour of ISO timestamps in source filenames") {
    using Policy = ssa::domain::SsaImportPolicy;
    REQUIRE(Policy::normalizeFilenameTimestamp("SSA_2026-07-15T10.00.xlsx") ==
            "2026-07-15 10:00:00");
    REQUIRE(Policy::normalizeFilenameTimestamp("SSA_2026-07-15T11.30.xlsx") ==
            "2026-07-15 11:30:00");
    REQUIRE(Policy::normalizeFilenameTimestamp("SSA_2026-07-15 09:05.xlsx") ==
            "2026-07-15 09:05:00");
    // Invalid datetimes are rejected; trailing digits are outside the substring match.
    REQUIRE(Policy::normalizeFilenameTimestamp("SSA_2026-07-15T25.00.xlsx").empty());
    REQUIRE(Policy::normalizeFilenameTimestamp("SSAs 2026-07-15 12.3456.xlsx") ==
            "2026-07-15 12:34:00");

    // Exports of the same day, one hour apart: the later one wins without a conflict.
    using Values = Policy::Values;
    const Values morning{{"numero_ssa", "202600035"},
                         {"descricao_ssa", "Morning"},
                         {"situacao", "APV"},
                         {"arquivo_origem", "SSA_2026-07-15T10.00.xlsx"}};
    auto later = morning;
    later["descricao_ssa"] = "Later";
    later["arquivo_origem"] = "SSA_2026-07-15T11.00.xlsx";
    const auto newerIncoming = Policy::merge(morning, later);
    const auto olderIncoming = Policy::merge(later, morning);
    REQUIRE_FALSE(newerIncoming.conflict);
    REQUIRE(newerIncoming.values.at("descricao_ssa") == "Later");
    REQUIRE_FALSE(olderIncoming.conflict);
    REQUIRE(olderIncoming.values.at("descricao_ssa") == "Later");
}

TEST_CASE("SSA import parses a valid timestamp after numeric filename noise") {
    std::string filename(4096, '7');
    filename += "_2026-07-15T09.08.xlsx";

    REQUIRE(ssa::domain::SsaImportPolicy::normalizeFilenameTimestamp(filename) ==
            "2026-07-15 09:08:00");
}

TEST_CASE("SSA filename timestamps follow Python pattern order and substring semantics") {
    using Policy = ssa::domain::SsaImportPolicy;
    const std::vector<std::pair<std::string, std::string>> cases{
        {"Consulta 14-07-2025_0343PM.xlsx", "2025-07-14 15:43:00"},
        {"14-07-2025-0343pm.xlsx", "2025-07-14 15:43:00"},
        {"14-07-2025 0343AM.xlsx", "2025-07-14 03:43:00"},
        {"14-07-2025 3:43pm.xlsx", "2025-07-14 15:43:00"},
        {"14-07-2025_3:43_PM.xlsx", "2025-07-14 15:43:00"},
        {"14-07-2025 03:43 PM.xlsx", "2025-07-14 15:43:00"},
        {"2025-07-14 15.43.xlsx", "2025-07-14 15:43:00"},
        {"2025_07/14t3:43:59.xlsx", "2025-07-14 03:43:00"},
        {"14-07-2025_1200AM.xlsx", "2025-07-14 00:00:00"},
        {"14-07-2025_1200PM.xlsx", "2025-07-14 12:00:00"},
        {"14-07-2025_0000AM.xlsx", "2025-07-14 00:00:00"},
        {"14-07-2025_0000PM.xlsx", "2025-07-14 12:00:00"},
        {"14-07-2025_1300AM.xlsx", "2025-07-14 13:00:00"},
        {"14-07-2025_1300PM.xlsx", ""},
        {"14-07-2025_2360AM.xlsx", ""},
        {"29-02-2025_0343PM.xlsx", ""},
        {"29-02-2024_0343PM.xlsx", "2024-02-29 15:43:00"},
        {"14-07-0000_0343PM.xlsx", ""},
        {"14-07-2025-3:43PM.xlsx", ""},
        {"2025-07-14_15.43.xlsx", ""},
        {"2025-07-14T3.43tail", "2025-07-14 03:43:00"},
        {"2026-01-01T10.00 14-07-2025 3:43PM 15-07-2025_0343PM", "2025-07-15 15:43:00"},
        {"2026-01-01T10.00 14-07-2025 3:43PM", "2025-07-14 15:43:00"},
        {"31-02-2025_0343PM 14-07-2025_0343PM", ""},
        {"31-02-2025_0343PM 14-07-2025 3:43PM", "2025-07-14 15:43:00"},
        {"31-02-2025 3:43PM 14-07-2025 3:43PM 2024-01-02T3.04", "2024-01-02 03:04:00"},
        {"2025-02-31T15.43 2025-07-14T15.43", ""},
    };
    for (const auto& [filename, expected] : cases) {
        INFO(filename);
        REQUIRE(Policy::normalizeFilenameTimestamp(filename) == expected);
    }
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

TEST_CASE("SSA import an older terminal row wins whole over a newer open row") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600024"},
                          {"descricao_ssa", "Current description"},
                          {"situacao", "APV"},
                          {"prazo_limite", "2026-07-30"},
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
    REQUIRE(result.openAfterTerminal);
    REQUIRE(result.values.at("situacao") == "STE");
    REQUIRE(result.values.at("descricao_ssa") == "Older execution");
    REQUIRE(result.values.at("data_planilha") == "2026-07-14");
    REQUIRE(result.values.at("arquivo_origem") == "SSAs executadas_14-07-2026.xlsx");
    // The terminal row lacks the field: the open row keeps it.
    REQUIRE(result.values.at("prazo_limite") == "2026-07-30");
}

TEST_CASE("SSA import a stored terminal row ignores a newer open row and flags it") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600025"},
                          {"descricao_ssa", "Execution"},
                          {"situacao", "SCA"},
                          {"data_planilha", "2026-07-14"},
                          {"arquivo_origem", "SSA_14-07-2026.xlsx"}};
    const Values incoming{{"numero_ssa", "202600025"},
                          {"descricao_ssa", "Stale reopen"},
                          {"situacao", "ADM"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSA_15-07-2026.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.openAfterTerminal);
    REQUIRE(result.values == existing);
    const auto text = ssa::domain::SsaImportPolicy::describeConflict(existing, incoming, result);
    REQUIRE(text == "Import open row newer than terminal: ssa=202600025 field=situacao "
                    "existing=\"SCA\" incoming=\"ADM\" existing_file=\"SSA_14-07-2026.xlsx\" "
                    "incoming_file=\"SSA_15-07-2026.xlsx\" existing_date=\"2026-07-14\" "
                    "incoming_date=\"2026-07-15\"");
}

TEST_CASE("SSA import an open row older than a terminal one is normal history, not flagged") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values terminal{{"numero_ssa", "202600027"},
                          {"descricao_ssa", "Execution"},
                          {"situacao", "STE"},
                          {"data_planilha", "2026-07-15"}};
    const Values open{{"numero_ssa", "202600027"},
                      {"descricao_ssa", "Before execution"},
                      {"situacao", "ADM"},
                      {"data_planilha", "2026-07-14"}};

    const auto stored = ssa::domain::SsaImportPolicy::merge(terminal, open);
    const auto promoted = ssa::domain::SsaImportPolicy::merge(open, terminal);

    REQUIRE_FALSE(stored.openAfterTerminal);
    REQUIRE_FALSE(stored.changed);
    REQUIRE_FALSE(promoted.openAfterTerminal);
    REQUIRE(promoted.values.at("situacao") == "STE");
    REQUIRE(promoted.values.at("descricao_ssa") == "Execution");
}

TEST_CASE("SSA import an undated terminal row wins over a dated open row in both orders") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values open{{"numero_ssa", "202600029"},
                      {"descricao_ssa", "Open"},
                      {"situacao", "APV"},
                      {"data_planilha", "2026-07-15"}};
    const Values terminal{
        {"numero_ssa", "202600029"}, {"descricao_ssa", "Closed"}, {"situacao", "STE"}};

    const auto promoted = ssa::domain::SsaImportPolicy::merge(open, terminal);
    const auto stored = ssa::domain::SsaImportPolicy::merge(terminal, open);

    REQUIRE(promoted.changed);
    REQUIRE(promoted.values.at("situacao") == "STE");
    REQUIRE(promoted.values.at("descricao_ssa") == "Closed");
    REQUIRE_FALSE(promoted.openAfterTerminal); // no date on the terminal row to compare
    REQUIRE_FALSE(stored.changed);
    REQUIRE(stored.values == terminal);
}

TEST_CASE("SSA import a terminal row between open rows gives the same result in any order") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const auto rowAt = [](const std::string& situacao, const std::string& date) {
        return Values{{"numero_ssa", "202600028"}, {"descricao_ssa", situacao + " " + date},
                      {"situacao", situacao},      {"prazo_limite", date},
                      {"data_planilha", date},     {"arquivo_origem", "SSA_" + date + ".xlsx"}};
    };
    const auto first = rowAt("ADM", "2026-07-13");
    const auto terminal = rowAt("STE", "2026-07-14");
    const auto last = rowAt("ADM", "2026-07-15");
    const auto fold = [](const std::vector<Values>& rows) {
        Values merged;
        for (const auto& row : rows) {
            merged = ssa::domain::SsaImportPolicy::merge(merged, row).values;
        }
        return merged;
    };

    const auto forward = fold({first, terminal, last});
    REQUIRE(forward == terminal);
    REQUIRE(fold({last, terminal, first}) == forward);
    REQUIRE(fold({terminal, last, first}) == forward);
    REQUIRE(fold({first, last, terminal}) == forward);
}

TEST_CASE("SSA import tie-break is total: a missing file date sorts before a present one") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    // Same sheet date; the row with a file date is the newer one, so it updates the other
    // instead of conflicting with it. The order of the two rows never changes the result.
    const Values undatedFile{{"numero_ssa", "202600026"},
                             {"descricao_ssa", "Current description"},
                             {"situacao", "APV"},
                             {"data_planilha", "2026-07-15"}};
    const Values datedFile{{"numero_ssa", "202600026"},
                           {"descricao_ssa", "Dated description"},
                           {"situacao", "APV"},
                           {"data_planilha", "2026-07-15"},
                           {"arquivo_origem", "SSA_15-07-2026.xlsx"},
                           {"data_criacao_arquivo", "2026-07-15 11:00:00"},
                           {"data_arquivo_origem", "2026-07-15 12:00:00"}};

    const auto forward = ssa::domain::SsaImportPolicy::merge(undatedFile, datedFile);
    REQUIRE_FALSE(forward.conflict);
    REQUIRE(forward.values.at("descricao_ssa") == "Dated description");

    const auto backward = ssa::domain::SsaImportPolicy::merge(datedFile, undatedFile);
    REQUIRE_FALSE(backward.conflict);
    REQUIRE_FALSE(backward.changed);
    REQUIRE(backward.values.at("descricao_ssa") == "Dated description");

    // The registration date never breaks a tie between rows that carry a sheet date.
    Values laterRegistration = undatedFile;
    laterRegistration["data_cadastro"] = "2026-01-02 00:00:00";
    laterRegistration["descricao_ssa"] = "Registered later";
    Values earlierRegistration = undatedFile;
    earlierRegistration["data_cadastro"] = "2026-01-01 00:00:00";
    const auto registration =
        ssa::domain::SsaImportPolicy::merge(earlierRegistration, laterRegistration);
    const auto reversed =
        ssa::domain::SsaImportPolicy::merge(laterRegistration, earlierRegistration);
    REQUIRE_FALSE(registration.conflict);
    REQUIRE(registration.values == reversed.values);
    REQUIRE(registration.values.at("descricao_ssa") == "Registered later");
}

TEST_CASE("SSA import breaks a same-minute filename tie with the file modification time") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values earlier{{"numero_ssa", "202600031"},
                         {"descricao_ssa", "Earlier export"},
                         {"situacao", "APV"},
                         {"arquivo_origem", "Consulta SSA - 28-05-2026_0306PM (1).xlsx"},
                         {"data_arquivo_origem", "2026-05-28 15:06:25"}};
    const Values later{{"numero_ssa", "202600031"},
                       {"descricao_ssa", "Later export"},
                       {"situacao", "APV"},
                       {"arquivo_origem", "Consulta SSA - 28-05-2026_0306PM (4).xlsx"},
                       {"data_arquivo_origem", "2026-05-28 15:06:57"}};

    const auto newerIncoming = ssa::domain::SsaImportPolicy::merge(earlier, later);
    const auto olderIncoming = ssa::domain::SsaImportPolicy::merge(later, earlier);

    REQUIRE_FALSE(newerIncoming.conflict);
    REQUIRE(newerIncoming.values.at("descricao_ssa") == "Later export");
    REQUIRE_FALSE(olderIncoming.conflict);
    REQUIRE_FALSE(olderIncoming.changed);
    REQUIRE(olderIncoming.values == later);
}

TEST_CASE("SSA import treats accent-only differences as the same value") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values plain{{"numero_ssa", "202600033"},
                       {"descricao_ssa", "Same"},
                       {"descricao_localizacao", "UNIDADE DE AQUISICAO DE DADOS - DAUC 03"},
                       {"situacao", "APV"},
                       {"arquivo_origem", "Consulta SSA - 20-02-2026_1129AM.xlsx"}};
    auto accented = plain;
    accented["descricao_localizacao"] = "UNIDADE DE AQUISIÇÃO DE DADOS - DAUC 03";
    auto different = plain;
    different["descricao_localizacao"] = "UNIDADE DE AQUISICAO DE DADOS - DAUC 04";
    different["arquivo_origem"] = "Consulta SSA - 20-02-2026_1129AM (1).xlsx";

    const auto upgrade = ssa::domain::SsaImportPolicy::merge(plain, accented);
    const auto keep = ssa::domain::SsaImportPolicy::merge(accented, plain);
    const auto conflict = ssa::domain::SsaImportPolicy::merge(plain, different);

    REQUIRE_FALSE(upgrade.conflict);
    REQUIRE(upgrade.changed);
    REQUIRE(upgrade.values.at("descricao_localizacao") == accented.at("descricao_localizacao"));
    REQUIRE_FALSE(keep.conflict);
    REQUIRE_FALSE(keep.changed);
    REQUIRE(keep.values == accented);
    // A different text with the same field count: content decides, never a conflict.
    REQUIRE_FALSE(conflict.conflict);
    REQUIRE(conflict.values == ssa::domain::SsaImportPolicy::merge(different, plain).values);
}

TEST_CASE("SSA import reports the first differing field of a repeat resolved by content") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600032"},
                          {"descricao_ssa", "Same"},
                          {"responsavel_execucao", "FELIPE"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "Consulta SSA - 28-05-2026_0306PM.xlsx"}};
    auto incoming = existing;
    incoming["responsavel_execucao"] = "PAULO";
    incoming["arquivo_origem"] = "Consulta SSA - 28-05-2026_0306PM (1).xlsx";

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE_FALSE(result.conflict);
    REQUIRE(result.repeatDiscarded);
    REQUIRE(result.conflictField == "responsavel_execucao");
    REQUIRE(ssa::domain::SsaImportPolicy::describeConflict(existing, incoming, result) ==
            "Import repeated SSA, resolved by content: ssa=202600032 "
            "field=responsavel_execucao existing=\"FELIPE\" incoming=\"PAULO\" "
            "existing_file=\"Consulta SSA - 28-05-2026_0306PM.xlsx\" "
            "incoming_file=\"Consulta SSA - 28-05-2026_0306PM (1).xlsx\"");
}

TEST_CASE("SSA import repeated in the same workbook keeps the row with more fields") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    // SAM exports one row per execution: only the execution fields differ.
    const Values first{{"numero_ssa", "202207778"},
                       {"descricao_ssa", "Same"},
                       {"situacao", "APV"},
                       {"responsavel_execucao", "FELIPE"},
                       {"descricao_execucao", "First execution"},
                       {"arquivo_origem", "SSAscomReprogramacoes_20-10-2026_1131AM.xlsx"},
                       {"data_arquivo_origem", "2026-10-20T11:31:40"}};
    auto second = first;
    second["responsavel_execucao"] = "PAULO";
    second["descricao_execucao"] = "Second execution";
    second["semana_executada"] = "202642";

    const auto result = ssa::domain::SsaImportPolicy::merge(first, second);
    const auto reversed = ssa::domain::SsaImportPolicy::merge(second, first);

    // The second row carries one more field, so it wins in either order.
    REQUIRE_FALSE(result.conflict);
    REQUIRE(result.changed);
    REQUIRE(result.values.at("responsavel_execucao") == "PAULO");
    REQUIRE(result.values.at("semana_executada") == "202642");
    REQUIRE_FALSE(reversed.changed);
    REQUIRE(reversed.repeatDiscarded);
    REQUIRE(reversed.conflictField == "descricao_execucao");
    REQUIRE(result.values == reversed.values);

    // Another workbook of the same instant follows the same content rule.
    auto siblingFile = second;
    siblingFile["arquivo_origem"] = "SSAscomReprogramacoes_20-10-2026_1131AM (1).xlsx";
    const auto sibling = ssa::domain::SsaImportPolicy::merge(first, siblingFile);
    REQUIRE_FALSE(sibling.conflict);
    REQUIRE(sibling.values.at("responsavel_execucao") == "PAULO");
}

TEST_CASE("SSA import equal snapshot resolves accents and transient states symmetrically") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values a{{"numero_ssa", "202600044"},
                   {"descricao_ssa", "caf\xC3\xA9"},
                   {"situacao", "APV"},
                   {"data_planilha", "2026-07-20"}};
    Values b = a;
    b["descricao_ssa"] = "caf\xC3\xAA";
    b["situacao"] = "SCS";
    const auto forward = ssa::domain::SsaImportPolicy::merge(a, b);
    const auto backward = ssa::domain::SsaImportPolicy::merge(b, a);
    REQUIRE(forward.values == backward.values);

    // DEL is escaped like Python's ensure_ascii, so "\x7f" sorts before "a".
    Values del = a;
    del["descricao_ssa"] = "\x7f";
    Values letter = a;
    letter["descricao_ssa"] = "a";
    REQUIRE(ssa::domain::SsaImportPolicy::merge(del, letter).values.at("descricao_ssa") == "a");
    REQUIRE(ssa::domain::SsaImportPolicy::merge(letter, del).values.at("descricao_ssa") == "a");
}

TEST_CASE("SSA import equal snapshot keeps the richer row in either order") {
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

    const auto sparseThenRich = ssa::domain::SsaImportPolicy::merge(sparse, rich);
    const auto richThenSparse = ssa::domain::SsaImportPolicy::merge(rich, sparse);

    REQUIRE_FALSE(sparseThenRich.conflict);
    REQUIRE_FALSE(richThenSparse.conflict);
    REQUIRE(sparseThenRich.changed);
    REQUIRE_FALSE(richThenSparse.changed);
    REQUIRE(sparseThenRich.values == rich);
    REQUIRE(richThenSparse.values == rich);
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
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("21/10/2025 11:10") ==
            "2025-10-21 11:10:00");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("2025-10-21T11:10:36") ==
            "2025-10-21 11:10:36");
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("prefix 21/10/2025 11:10:36")
                .empty());
    REQUIRE(ssa::domain::SsaImportPolicy::normalizeSnapshotTimestamp("21/10/2025 11:10:36 suffix")
                .empty());
}

TEST_CASE("SSA import date text preserves ISO representation and normalizes legacy values") {
    using Policy = ssa::domain::SsaImportPolicy;
    REQUIRE(Policy::normalizeDateText(" 2026-07-13 ") == "2026-07-13");
    REQUIRE(Policy::normalizeDateText("2026-07-13T13:20:00") == "2026-07-13T13:20:00");
    REQUIRE(Policy::normalizeDateText("13/07/2026 13:20:00") == "2026-07-13 13:20:00");
    REQUIRE(Policy::normalizeDateText("invalid").empty());
}

TEST_CASE("SSA upsert dates store whole seconds with day-first slash parsing") {
    using Policy = ssa::domain::SsaImportPolicy;
    for (const auto& [input, expected] : std::vector<std::pair<std::string, std::string>>{
             {"09/10/2025", "2025-10-09 00:00:00"},
             {"2025-07-14T15:43:12", "2025-07-14 15:43:12"},
             {"2025-07-14", "2025-07-14 00:00:00"},
             {"2025-07-14 15:43:12", "2025-07-14 15:43:12"},
             {"09/10/2025 15:43", "2025-10-09 15:43:00"},
             {"09/10/2025 15:43:12", "2025-10-09 15:43:12"},
             {"10/14/2025", "2025-10-14 00:00:00"},
             {"10/14/2025 15:43", "2025-10-14 15:43:00"},
             {"10/14/2025 15:43:12", "2025-10-14 15:43:12"},
             {"14-07-2025", "2025-07-14 00:00:00"},
             {" 2025-07-14T15:43:12.999999 ", "2025-07-14 15:43:12"},
             {"14/07/2025 15:43:12.123", "2025-07-14 15:43:12"}}) {
        INFO(input);
        REQUIRE(Policy::normalizeUpsertDate(input) == expected);
        REQUIRE(Policy::normalizeUpsertDate(expected) == expected);
    }
    for (const auto* input :
         {"", "bad-date", "31/02/2025", "2025-07-14T25:00:00", "2025-07-14T15:43:12.",
          "2025-07-14T15:43:12.1junk", "45205.5", "1"}) {
        INFO(input);
        REQUIRE(Policy::normalizeUpsertDate(input).empty());
    }
}

TEST_CASE("SSA upsert date keys cover text dates but exclude source metadata") {
    using Policy = ssa::domain::SsaImportPolicy;
    for (const auto* key : {"data_cadastro", "prazo_limite", "data_limite", "desde", "desde_1",
                            "desde_2", "ate", "ate_1", "ate_2", "data_inicio_programada",
                            "data_programacao", "data_inicio_reprogramada", "data_reprogramacao",
                            "instalacao_estimada", "executado", "concluido"}) {
        INFO(key);
        REQUIRE(Policy::isUpsertDateColumn(key));
        const Policy::Values existing{{"numero_ssa", "202600001"}, {key, "09/10/2025"}};
        const Policy::Values incoming{{"numero_ssa", "202600001"}, {key, "2025-10-09 00:00:00"}};
        REQUIRE_FALSE(Policy::merge(existing, incoming).changed);
    }
    for (const auto* key :
         {"data_planilha", "data_arquivo_origem", "data_criacao_arquivo", "descricao_ssa"}) {
        INFO(key);
        REQUIRE_FALSE(Policy::isUpsertDateColumn(key));
    }
    for (const auto* key : {"instalacao_estimada", "executado", "concluido"}) {
        INFO(key);
        REQUIRE(ssa::domain::ColumnCatalog::find(key)->type == ssa::domain::ColumnType::Text);
    }
}

TEST_CASE("SSA import compares dates, numbers and codes canonically, keeping the stored value") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    // Python parity (A6/C2): the same value in another spelling is neither written nor counted.
    const Values stored{{"numero_ssa", "202600043"},
                        {"descricao_ssa", "Same"},
                        {"situacao", "APV"},
                        {"prazo_limite", "01/01/2026 00:00:00"},
                        {"semana_executada", "202601"},
                        {"data_planilha", "2026-07-20"}};
    Values incoming = stored;
    incoming["prazo_limite"] = "2026-01-01";
    incoming["semana_executada"] = "202601.0";
    incoming["data_planilha"] = "2026-07-21";

    const auto result = ssa::domain::SsaImportPolicy::merge(stored, incoming);
    REQUIRE(result.metadataOnly);
    REQUIRE(result.values.at("prazo_limite") == "01/01/2026 00:00:00");
    REQUIRE(result.values.at("semana_executada") == "202601");

    // An exponent is the same number; a code with leading zeros is a different text.
    incoming["semana_executada"] = "2.02601e5";
    REQUIRE(ssa::domain::SsaImportPolicy::merge(stored, incoming).metadataOnly);
    Values code = stored;
    code["descricao_ssa"] = "007";
    Values recode = code;
    recode["descricao_ssa"] = "7";
    recode["data_planilha"] = "2026-07-21";
    const auto leading = ssa::domain::SsaImportPolicy::merge(code, recode);
    REQUIRE_FALSE(leading.metadataOnly);
    REQUIRE(leading.values.at("descricao_ssa") == "7");

    // Exact decimals, no floating point: 2^53 and 2^53 + 1 are different values.
    incoming["semana_executada"] = "9007199254740993";
    Values large = stored;
    large["semana_executada"] = "9007199254740992";
    const auto precise = ssa::domain::SsaImportPolicy::merge(large, incoming);
    REQUIRE(precise.changed);
    REQUIRE_FALSE(precise.metadataOnly);
    REQUIRE(precise.values.at("semana_executada") == "9007199254740993");
}

TEST_CASE("SSA import rows without a report date never conflict") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values dated{{"numero_ssa", "202600041"},
                       {"descricao_ssa", "Dated"},
                       {"situacao", "APV"},
                       {"data_planilha", "2026-07-20"}};
    const Values undated{
        {"numero_ssa", "202600041"}, {"descricao_ssa", "Undated"}, {"situacao", "APV"}};

    const auto keep = ssa::domain::SsaImportPolicy::merge(dated, undated);
    REQUIRE_FALSE(keep.conflict);
    REQUIRE_FALSE(keep.changed);
    REQUIRE(keep.values == dated);

    const auto update = ssa::domain::SsaImportPolicy::merge(undated, dated);
    REQUIRE_FALSE(update.conflict);
    REQUIRE(update.changed);
    REQUIRE(update.values.at("descricao_ssa") == "Dated");

    const Values undatedV2{
        {"numero_ssa", "202600041"}, {"descricao_ssa", "Undated v2"}, {"situacao", "APV"}};
    const auto tie = ssa::domain::SsaImportPolicy::merge(undated, undatedV2);
    REQUIRE_FALSE(tie.conflict);
    REQUIRE(tie.values.at("descricao_ssa") == "Undated v2");
}

TEST_CASE("SSA import ignores case in coded fields but not in free text") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values existing{{"numero_ssa", "202600042"},
                          {"descricao_ssa", "Bomba"},
                          {"situacao", "ADM - Aguardando"},
                          {"setor_executor", "MEC"},
                          {"data_planilha", "2026-07-20"}};
    Values incoming = existing;
    incoming["situacao"] = "adm - aguardando";
    incoming["setor_executor"] = "mec";

    const auto sameDay = ssa::domain::SsaImportPolicy::merge(existing, incoming);
    REQUIRE_FALSE(sameDay.conflict);
    REQUIRE_FALSE(sameDay.changed);

    incoming["data_planilha"] = "2026-07-21";
    const auto newer = ssa::domain::SsaImportPolicy::merge(existing, incoming);
    REQUIRE(newer.metadataOnly); // only the sheet date moved; the coded values are the same
    REQUIRE(newer.values.at("situacao") == "ADM - Aguardando");
    REQUIRE(newer.values.at("setor_executor") == "MEC");

    incoming["descricao_ssa"] = "bomba";
    const auto freeText = ssa::domain::SsaImportPolicy::merge(existing, incoming);
    REQUIRE(freeText.changed);
    REQUIRE(freeText.values.at("descricao_ssa") == "bomba");
}

TEST_CASE("SSA import snapshot prefers the modification time over the creation time") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    // A staged copy gets a fresh creation time; the modification time is the report time.
    const Values existing{{"numero_ssa", "202600031"},
                          {"descricao_ssa", "Older report"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "relatorio.xlsx"},
                          {"data_arquivo_origem", "2026-07-20 10:00:00"},
                          {"data_criacao_arquivo", "2026-07-29 10:00:00"}};
    const Values incoming{{"numero_ssa", "202600031"},
                          {"descricao_ssa", "Newer report"},
                          {"situacao", "APV"},
                          {"arquivo_origem", "relatorio.xlsx"},
                          {"data_arquivo_origem", "2026-07-21 10:00:00"},
                          {"data_criacao_arquivo", "2026-07-01 10:00:00"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE(result.changed);
    REQUIRE(result.values.at("descricao_ssa") == "Newer report");
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

TEST_CASE("terminal SSA is never overwritten by newer or older snapshots") {
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

    // Python parity: a stored STE/SCA row is never overwritten, not even indicators.
    REQUIRE_FALSE(advanced.changed);
    REQUIRE(advanced.values == terminal);
    REQUIRE_FALSE(ignored.changed);
    REQUIRE(ignored.values == terminal);
}

TEST_CASE("terminal SSA ignores newer execution planning and description fields") {
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
                          {"execucao_simples", "Sim"},
                          {"descricao_execucao", "Execucao parcial 2"},
                          {"data_planilha", "2026-07-15"},
                          {"arquivo_origem", "SSAs executadas_15-07-2026.xlsx"}};

    const auto result = ssa::domain::SsaImportPolicy::merge(existing, incoming);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.values == existing);
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

TEST_CASE("equal SSA snapshot never changes a terminal row in either order") {
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

    const auto existingThenIncoming = ssa::domain::SsaImportPolicy::merge(existing, incoming);
    const auto incomingThenExisting = ssa::domain::SsaImportPolicy::merge(incoming, existing);

    REQUIRE_FALSE(existingThenIncoming.conflict);
    REQUIRE_FALSE(incomingThenExisting.conflict);
    REQUIRE_FALSE(existingThenIncoming.changed);
    REQUIRE_FALSE(incomingThenExisting.changed);
    REQUIRE(existingThenIncoming.values == existing);
    REQUIRE(incomingThenExisting.values == incoming);
}

TEST_CASE("SSA import journals a differing repeat of a frozen terminal row") {
    using Values = ssa::domain::SsaImportPolicy::Values;
    const Values alpha{{"numero_ssa", "202600030"},
                       {"descricao_ssa", "Alpha"},
                       {"situacao", "STE"},
                       {"data_planilha", "2026-07-14"}};
    auto zulu = alpha;
    zulu["descricao_ssa"] = "Zulu";

    const auto result = ssa::domain::SsaImportPolicy::merge(alpha, zulu);

    REQUIRE_FALSE(result.changed);
    REQUIRE(result.values == alpha);
    REQUIRE(result.repeatDiscarded);
    REQUIRE(result.conflictField == "descricao_ssa");
}
