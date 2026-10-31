#include "domain/SsaImportGroup.h"
#include "domain/SsaImportPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

    using ssa::domain::SsaImportGroup;
    using Values = ssa::domain::SsaImportPolicy::Values;

    struct Source {
        Values row;
        std::string name;
        std::size_t source = 0; // defaults to the position in the list
    };

    // Folds the sources in every order and requires one result: values and winner.
    SsaImportGroup foldInEveryOrder(std::vector<Source> sources) {
        std::vector<std::size_t> order(sources.size());
        for (std::size_t index = 0; index < order.size(); ++index) {
            order[index] = index;
            sources[index].source = sources[index].source == 0 ? index : sources[index].source;
        }
        const auto fold = [&](const std::vector<std::size_t>& sequence) {
            SsaImportGroup group;
            for (const auto index : sequence) {
                group.add(sources[index].row, sources[index].source, sources[index].name);
            }
            return group;
        };
        const auto first = fold(order);
        while (std::ranges::next_permutation(order).found) {
            const auto other = fold(order);
            REQUIRE(other.values() == first.values());
            REQUIRE(other.winnerSource() == first.winnerSource());
            REQUIRE(other.newestOpen() == first.newestOpen());
            REQUIRE(other.oldestTerminal() == first.oldestTerminal());
            REQUIRE(other.resolvedByContent() == first.resolvedByContent());
        }
        return first;
    }

    Values rowAt(const std::string& situacao, const std::string& date) {
        return {{"numero_ssa", "202600100"},
                {"situacao", situacao},
                {"descricao_ssa", situacao + " " + date},
                {"data_planilha", date},
                {"arquivo_origem", "SSA_" + date + ".xlsx"}};
    }

} // namespace

TEST_CASE("SSA repeat gate compares folded business fields without materializing the group") {
    using Policy = ssa::domain::SsaImportPolicy;
    auto older = rowAt("ADM", "2026-09-01");
    older["setor_executor"] = "SETOR";
    auto newest = rowAt("ADM", "2026-09-02");
    SsaImportGroup group;
    group.add(older, 0, "older");
    group.add(newest, 1, "newest");
    REQUIRE_FALSE(group.hasRepeatDifference(newest));
    newest["setor_executor"] = "setor";
    REQUIRE_FALSE(group.hasRepeatDifference(newest));
    newest["setor_executor"] = "OTHER";
    REQUIRE(group.hasRepeatDifference(newest));
    const auto current = group.values();
    const auto merged = Policy::merge(current, newest);
    REQUIRE(merged.repeatDiscarded);
    REQUIRE(merged.conflictField == "setor_executor");
    REQUIRE(Policy::describeConflict(current, newest, merged) ==
            "Import repeated SSA, resolved by content: ssa=202600100 field=setor_executor "
            "existing=\"SETOR\" incoming=\"OTHER\" existing_file=\"SSA_2026-09-02.xlsx\" "
            "incoming_file=\"SSA_2026-09-02.xlsx\"");
}

TEST_CASE("SSA repeat gate ignores metadata status and empty or absent fields") {
    auto row = rowAt("ADM", "2026-09-01");
    SsaImportGroup group;
    group.add(row, 0, "source");
    row["arquivo_origem"] = "other.xlsx";
    row["situacao"] = "STE";
    row["data_criacao_arquivo"] = "2026-08-01";
    row["descricao_ssa"].clear();
    row["setor_executor"] = "NEW";
    REQUIRE_FALSE(group.hasRepeatDifference(row));
    row["descricao_ssa"] = "Changed";
    REQUIRE(group.hasRepeatDifference(row));
}

TEST_CASE("SSA group originals follow each field winner in every input order") {
    auto old = rowAt("ADM", "2026-09-01");
    old["setor_executor"] = "SETOR";
    old["valores_originais"] =
        R"({"descricao_ssa":" old ","setor_executor":" SETOR ","solicitante":"N/A"})";
    auto newest = rowAt("ADM", "2026-09-02");
    newest["valores_originais"] = R"({"descricao_ssa":null,"solicitante":"-"})";
    auto group = foldInEveryOrder({{old, "a.xlsx"}, {newest, "b.xlsx"}});
    REQUIRE(group.values().at("valores_originais") ==
            R"({"descricao_ssa":null,"setor_executor":" SETOR ","solicitante":"-"})");

    newest["setor_executor"] = "SETOR";
    newest.erase("valores_originais");
    group = foldInEveryOrder({{old, "a.xlsx"}, {newest, "b.xlsx"}});
    REQUIRE(group.values().at("valores_originais") == R"({"solicitante":"N/A"})");
}

TEST_CASE("SSA group empty originals never displace an older nonempty field") {
    auto old = rowAt("ADM", "2026-09-01");
    old["solicitante"] = "Pessoa";
    old["valores_originais"] = R"({"solicitante":" Pessoa "})";
    auto newest = rowAt("ADM", "2026-09-02");
    newest["valores_originais"] = R"({"solicitante":"N/A","dropped":"x"})";
    const auto group = foldInEveryOrder({{old, "a.xlsx"}, {newest, "b.xlsx"}});
    REQUIRE(group.values().at("solicitante") == "Pessoa");
    REQUIRE(group.values().at("valores_originais") == R"({"solicitante":" Pessoa "})");
}

TEST_CASE("SSA import group takes each field from the newest row that has it, in any order") {
    auto oldest = rowAt("ADM", "2026-07-13");
    oldest["setor_executor"] = "MEL1";
    oldest["prazo_limite"] = "2026-07-20";
    auto middle = rowAt("APV", "2026-07-14");
    middle["prazo_limite"] = "2026-07-25";
    const auto newest = rowAt("ASE", "2026-07-15");

    const auto group =
        foldInEveryOrder({{oldest, "a.xlsx"}, {middle, "b.xlsx"}, {newest, "c.xlsx"}});

    auto expected = newest;
    expected["setor_executor"] = "MEL1";
    expected["prazo_limite"] = "2026-07-25";
    REQUIRE(group.values() == expected);
    REQUIRE(group.winnerSource() == 2);
    REQUIRE(group.rows() == 3);
}

TEST_CASE("SSA import group keeps only the newest terminal rows, whole, and drops open rows") {
    auto ste = rowAt("STE", "2026-01-10");
    ste["setor_executor"] = "MEL1";
    auto sca = rowAt("SCA", "2026-06-10");
    auto adm = rowAt("ADM", "2026-03-10");
    adm["prazo_limite"] = "2026-03-30";

    const auto group = foldInEveryOrder({{ste, "jan.xlsx"}, {adm, "mar.xlsx"}, {sca, "jun.xlsx"}});

    // Neither the older terminal nor the open row fills the winning terminal row.
    REQUIRE(group.values() == sca);
    REQUIRE(group.winnerSource() == 2);
    // The open row of March is newer than the January terminal: a stale file or wrong date.
    REQUIRE(ssa::domain::SsaImportPolicy::isOpenAfterTerminal(group.newestOpen(),
                                                              group.oldestTerminal()));
    REQUIRE(group.oldestTerminal().at("data_planilha") == "2026-01-10");
}

TEST_CASE("SSA import group lets terminal rows of equal precedence fill each other") {
    auto first = rowAt("STE", "2026-07-14");
    first["setor_executor"] = "MEL1";
    auto second = rowAt("STE", "2026-07-14");
    second["prazo_limite"] = "2026-07-30";
    const auto open = rowAt("ADM", "2026-07-13");

    const auto group = foldInEveryOrder({{first, "a.xlsx"}, {second, "b.xlsx"}, {open, "c.xlsx"}});

    REQUIRE(group.values().at("setor_executor") == "MEL1");
    REQUIRE(group.values().at("prazo_limite") == "2026-07-30");
    REQUIRE(group.values().at("situacao") == "STE");
    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isOpenAfterTerminal(group.newestOpen(),
                                                                    group.oldestTerminal()));
}

TEST_CASE("SSA import group lets an undated terminal row win over dated open rows") {
    const Values terminal{
        {"numero_ssa", "202600100"}, {"situacao", "SCA"}, {"descricao_ssa", "Canceled"}};

    const auto group =
        foldInEveryOrder({{rowAt("ADM", "2026-07-13"), "a.xlsx"}, {terminal, "b.xlsx"}});

    REQUIRE(group.values() == terminal);
    REQUIRE(group.winnerSource() == 1);
    // No date on the terminal row: nothing to compare, no warning.
    REQUIRE_FALSE(ssa::domain::SsaImportPolicy::isOpenAfterTerminal(group.newestOpen(),
                                                                    group.oldestTerminal()));
}

TEST_CASE("SSA import group attributes identical rows of two files by name, not by order") {
    const auto row = rowAt("ADM", "2026-07-13");
    auto spelled = row;
    spelled["descricao_ssa"] = "  " + row.at("descricao_ssa") + " ";

    const auto group = foldInEveryOrder({{row, "b.xlsx"}, {spelled, "a.xlsx"}});

    REQUIRE(group.values() == row);
    REQUIRE(group.winnerSource() == 0);
    REQUIRE_FALSE(group.resolvedByContent());
}

TEST_CASE("SSA import group resolves rows of equal precedence by content") {
    auto richer = rowAt("ADM", "2026-07-13");
    richer["setor_executor"] = "MEL1";
    auto poorer = rowAt("ADM", "2026-07-13");
    poorer["descricao_ssa"] = "Other description";

    const auto group =
        foldInEveryOrder({{poorer, "b.xlsx"}, {richer, "a.xlsx"}}); // the name favors poorer

    REQUIRE(group.values() == richer);
    REQUIRE(group.winnerSource() == 1);
    REQUIRE(group.resolvedByContent());
}

TEST_CASE("SSA import group A then B then A gives A with the newest file metadata") {
    auto first = rowAt("ADM", "2026-07-13");
    auto changed = rowAt("ADM", "2026-07-14");
    changed["descricao_ssa"] = "B";
    auto again = rowAt("ADM", "2026-07-15");
    again["descricao_ssa"] = first.at("descricao_ssa");

    const auto group =
        foldInEveryOrder({{first, "13.xlsx"}, {changed, "14.xlsx"}, {again, "15.xlsx"}});

    REQUIRE(group.values() == again);
    REQUIRE(group.winnerSource() == 2);
}

TEST_CASE("SSA import group picks one spelling of a value repeated in the same file") {
    auto lower = rowAt("ADM", "2026-07-13");
    lower["setor_executor"] = "mel1";
    auto upper = lower;
    upper["setor_executor"] = "MEL1";

    const auto group = foldInEveryOrder({{lower, "same.xlsx", 7}, {upper, "same.xlsx", 7}});

    // Same canonical row from the same file: the larger spelling, whatever the order.
    REQUIRE(group.values().at("setor_executor") == "mel1");
    REQUIRE_FALSE(group.resolvedByContent());
}

TEST_CASE("SSA import group reports a content tie only at the winning precedence") {
    auto first = rowAt("ADM", "2026-07-13");
    first["descricao_ssa"] = "A";
    auto second = rowAt("ADM", "2026-07-13");
    second["descricao_ssa"] = "B";
    auto newest = rowAt("ADM", "2026-07-14");

    REQUIRE_FALSE(foldInEveryOrder({{first, "a.xlsx"}, {second, "b.xlsx"}, {newest, "c.xlsx"}})
                      .resolvedByContent());
    REQUIRE(foldInEveryOrder({{first, "a.xlsx"}, {second, "b.xlsx"}}).resolvedByContent());
}

TEST_CASE("SSA import group dating fields of a row spelled twice do not depend on order") {
    auto iso = rowAt("ADM", "2026-07-13");
    auto local = iso;
    local["data_planilha"] = "13/07/2026";
    auto terminalIso = rowAt("STE", "2026-07-12");
    auto terminalLocal = terminalIso;
    terminalLocal["data_planilha"] = "12/07/2026";

    const auto group = foldInEveryOrder({{iso, "same.xlsx", 7},
                                         {local, "same.xlsx", 7},
                                         {terminalIso, "t.xlsx", 9},
                                         {terminalLocal, "t.xlsx", 9}});

    REQUIRE(group.newestOpen().at("data_planilha") == "2026-07-13");
    REQUIRE(group.oldestTerminal().at("data_planilha") == "2026-07-12");
}
