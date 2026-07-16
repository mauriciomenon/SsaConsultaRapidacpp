#include "infra/import/DerivadasSourceReader.h"
#include "infra/import/LegacySpreadsheetConverter.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivadasPort.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <miniz.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <latch>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace {

    std::shared_ptr<ssa::infra::importing::LegacySpreadsheetConverter>
    unavailableLegacyConverter() {
        return std::make_shared<ssa::infra::importing::LegacySpreadsheetConverter>(
            std::filesystem::path{}, nullptr);
    }

    struct Fixture {
        QTemporaryDir directory;
        std::filesystem::path databasePath;

        Fixture() {
            REQUIRE(directory.isValid());
            databasePath = std::filesystem::path{directory.path().toStdString()} / "ssa.sqlite";
            ssa::infra::sqlite::SqliteConnection connection(
                databasePath, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);
            REQUIRE(sqlite3_exec(connection.handle(),
                                 "CREATE TABLE ssa_table ("
                                 "numero_ssa TEXT PRIMARY KEY, derivada_de TEXT);"
                                 "INSERT INTO ssa_table VALUES"
                                 "('202600001', NULL),"
                                 "('202600002', NULL),"
                                 "('202600003', NULL),"
                                 "('202600004', NULL);",
                                 nullptr, nullptr, nullptr) == SQLITE_OK);
        }

        [[nodiscard]] std::filesystem::path path(const std::string& name) const {
            return std::filesystem::path{directory.path().toStdString()} / name;
        }

        [[nodiscard]] std::string parentOf(const std::string& child) const {
            ssa::infra::sqlite::SqliteConnection connection(databasePath);
            ssa::infra::sqlite::SqliteStatement statement(
                connection.handle(), "SELECT COALESCE(derivada_de, '') FROM ssa_table "
                                     "WHERE numero_ssa = ?");
            statement.bindTextOneBased(1, child);
            REQUIRE(statement.step());
            return statement.columnText(0);
        }

        void setParent(const std::string& child, const std::string& parent) const {
            ssa::infra::sqlite::SqliteConnection connection(
                databasePath, ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
            ssa::infra::sqlite::SqliteStatement statement(
                connection.handle(), "UPDATE ssa_table SET derivada_de = ? WHERE numero_ssa = ?");
            statement.bindTextOneBased(1, parent);
            statement.bindTextOneBased(2, child);
            statement.executeAndReset();
        }
    };

    void writeText(const std::filesystem::path& path, const std::string& text) {
        std::ofstream output(path, std::ios::binary);
        REQUIRE(output.is_open());
        output << text;
        REQUIRE(output.good());
    }

    void addZipEntry(mz_zip_archive& zip, const std::string& path, const std::string& content) {
        REQUIRE(mz_zip_writer_add_mem(&zip, path.c_str(), content.data(), content.size(),
                                      MZ_BEST_COMPRESSION) != 0);
    }

    std::string inlineCell(const std::string& reference, const std::string& value) {
        return "<c r=\"" + reference + "\" t=\"inlineStr\"><is><t>" + value + "</t></is></c>";
    }

    std::string sheetXml(const std::vector<std::vector<std::string>>& rows) {
        std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)xml";
        for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            xml += "<row r=\"" + std::to_string(rowIndex + 1) + "\">";
            for (std::size_t column = 0; column < rows[rowIndex].size(); ++column) {
                xml += inlineCell(std::string(1, static_cast<char>('A' + column)) +
                                      std::to_string(rowIndex + 1),
                                  rows[rowIndex][column]);
            }
            xml += "</row>";
        }
        return xml + "</sheetData></worksheet>";
    }

    void writeWorkbook(const std::filesystem::path& path,
                       const std::vector<std::vector<std::vector<std::string>>>& sheets) {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0);
        addZipEntry(zip, "[Content_Types].xml",
                    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
</Types>)xml");
        std::string workbook = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>)xml";
        std::string relationships = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)xml";
        for (std::size_t index = 0; index < sheets.size(); ++index) {
            const auto number = std::to_string(index + 1);
            workbook += "<sheet name=\"Sheet" + number + "\" sheetId=\"" + number +
                        "\" r:id=\"rId" + number + "\"/>";
            relationships +=
                "<Relationship Id=\"rId" + number +
                "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                "worksheet\" Target=\"worksheets/sheet" +
                number + ".xml\"/>";
            addZipEntry(zip, "xl/worksheets/sheet" + number + ".xml", sheetXml(sheets[index]));
        }
        addZipEntry(zip, "xl/workbook.xml", workbook + "</sheets></workbook>");
        addZipEntry(zip, "xl/_rels/workbook.xml.rels", relationships + "</Relationships>");
        REQUIRE(mz_zip_writer_finalize_archive(&zip) != 0);
        REQUIRE(mz_zip_writer_end(&zip) != 0);
    }

    ssa::ports::ImportDerivationsRequest requestFor(const std::filesystem::path& path) {
        return {{path}};
    }

} // namespace

TEST_CASE("derivadas import applies CSV edges and preserves an absent parent") {
    const Fixture fixture;
    const auto source = fixture.path("derivadas.csv");
    writeText(source, "ssa_mae,ssa_filha\n202699999,202600001\n202699999,202600001\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.message.find("1 applied") != std::string::npos);
    REQUIRE(result.message.find("1 duplicate") != std::string::npos);
    REQUIRE(result.message.find("1 missing parent") != std::string::npos);
    REQUIRE(fixture.parentOf("202600001") == "202699999");
}

TEST_CASE("derivadas import preserves relations outside a partial batch") {
    const Fixture fixture;
    fixture.setParent("202600004", "202600003");
    const auto source = fixture.path("partial.csv");
    writeText(source, "parent_ssa,child_ssa\n202600001,202600002\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
    REQUIRE(fixture.parentOf("202600004") == "202600003");
}

TEST_CASE("derivadas import replaces the parent only for a child present in the batch") {
    const Fixture fixture;
    fixture.setParent("202600002", "202600001");
    const auto source = fixture.path("replacement.csv");
    writeText(source, "parent_ssa,child_ssa\n202600003,202600002\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600003");
}

TEST_CASE("derivadas import reads TXT and TSV as explicit sources") {
    const Fixture fixture;
    const auto txt = fixture.path("first.txt");
    const auto tsv = fixture.path("second.tsv");
    writeText(txt, "parent_ssa,child_ssa\n202600001,202600002\n");
    writeText(tsv, "numero_ssa_mae\tnumero_ssa_filha\n202600002\t202600003\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations({{txt, tsv}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
    REQUIRE(fixture.parentOf("202600003") == "202600002");
}

TEST_CASE("derivadas import reads every worksheet from XLSX and XLSM") {
    const Fixture fixture;
    const auto xlsx = fixture.path("first.xlsx");
    const auto xlsm = fixture.path("second.xlsm");
    writeWorkbook(xlsx, {{{"cover"}}, {{"parent_ssa", "child_ssa"}, {"202600001", "202600002"}}});
    writeWorkbook(xlsm, {{{"ssa_pai", "ssa_derivada"}, {"202600002", "202600003"}}});
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations({{xlsx, xlsm}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
    REQUIRE(fixture.parentOf("202600003") == "202600002");
}

TEST_CASE("derivadas import reads the shifted visual matrix used by the Python workflow") {
    const Fixture fixture;
    const auto source = fixture.path("SSAs Derivadas e Relacionadas.xlsx");
    writeWorkbook(source, {{{"SSAs Derivadas e Relacionadas", "", ""},
                            {"N\xC3\xBAmero da SSA",
                             "Rela\xC3\xA7"
                             "ao",
                             "N\xC3\xBAmero da SSA"},
                            {"202600001", "", ""},
                            {"202600002", "Derivada da", "202600001"},
                            {"202600003", "Derivada da", "202600001"}}});
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
    REQUIRE(fixture.parentOf("202600003") == "202600001");
}

TEST_CASE("derivadas import normalizes the documented SSA number forms") {
    const Fixture fixture;
    const auto source = fixture.path("normalized.csv");
    writeText(source, "parent_ssa,numero_ssa\n 2026-00001 ,202600002.0\n202600002, 2026-00003 \n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
    REQUIRE(fixture.parentOf("202600003") == "202600002");
}

TEST_CASE("derivadas import rejects sources without parse evidence") {
    const Fixture fixture;
    const auto empty = fixture.path("empty.csv");
    const auto unknown = fixture.path("unknown.csv");
    const auto cover = fixture.path("cover.xlsx");
    writeText(empty, "");
    writeText(unknown, "other,value\na,b\n");
    writeWorkbook(cover, {{{"SSAs Derivadas e Relacionadas"}}});
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    for (const auto& source : {empty, unknown, cover}) {
        const auto result = port.importDerivations(requestFor(source));
        REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
        REQUIRE(fixture.parentOf("202600001").empty());
    }
}

TEST_CASE("derivadas import rejects a self loop atomically") {
    const Fixture fixture;
    const auto valid = fixture.path("valid.csv");
    const auto invalid = fixture.path("self-loop.csv");
    writeText(valid, "parent_ssa,child_ssa\n202600001,202600002\n");
    writeText(invalid, "parent_ssa,child_ssa\n202600003,202600003\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations({{valid, invalid}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("self-loop") != std::string::npos);
    REQUIRE(fixture.parentOf("202600002").empty());
}

TEST_CASE("derivadas import rejects multiple parents for one child atomically") {
    const Fixture fixture;
    const auto source = fixture.path("multiparent.csv");
    writeText(source, "parent_ssa,child_ssa\n202600001,202600003\n202600002,202600003\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("multiple parents") != std::string::npos);
    REQUIRE(fixture.parentOf("202600003").empty());
}

TEST_CASE("derivadas import rejects a missing child without clearing existing data") {
    const Fixture fixture;
    const auto initial = fixture.path("initial.csv");
    const auto invalid = fixture.path("missing-child.csv");
    writeText(initial, "parent_ssa,child_ssa\n202600001,202600002\n");
    writeText(invalid, "parent_ssa,child_ssa\n202600003,202699999\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());
    REQUIRE(port.importDerivations(requestFor(initial)).ok());

    const auto result = port.importDerivations(requestFor(invalid));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("missing child") != std::string::npos);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
}

TEST_CASE("derivadas import keeps an earlier valid source atomic when a later source is invalid") {
    const Fixture fixture;
    const auto valid = fixture.path("valid.csv");
    const auto unsupported = fixture.path("unsupported.json");
    writeText(valid, "parent_ssa,child_ssa\n202600001,202600002\n");
    writeText(unsupported, "{}\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());

    const auto result = port.importDerivations({{valid, unsupported}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(fixture.parentOf("202600002").empty());
}

TEST_CASE("derivadas import reports no changes when an edge is already stored") {
    const Fixture fixture;
    const auto source = fixture.path("same.csv");
    writeText(source, "parent_ssa,child_ssa\n202600001,202600002\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());
    REQUIRE(port.importDerivations(requestFor(source)).ok());

    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::NoChanges);
    REQUIRE(fixture.parentOf("202600002") == "202600001");
}

TEST_CASE("derivadas import observes a stopped token before changing data") {
    const Fixture fixture;
    const auto source = fixture.path("canceled.csv");
    writeText(source, "parent_ssa,child_ssa\n202600001,202600002\n");
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = port.importDerivations(requestFor(source), stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(fixture.parentOf("202600002").empty());
}

TEST_CASE("derivadas import cancels during parsing and leaves the database reusable") {
    const Fixture fixture;
    const auto source = fixture.path("large.csv");
    std::string content = "parent_ssa,child_ssa\n";
    content.reserve(6'000'000);
    for (std::size_t index = 0; index < 250'000; ++index) {
        content += "202600001,202600002\n";
    }
    writeText(source, content);
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return port.importDerivations(requestFor(source), stopSource.get_token());
    });

    REQUIRE(operation.wait_for(std::chrono::milliseconds{1}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    REQUIRE(operation.get().status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(fixture.parentOf("202600002").empty());

    writeText(source, "parent_ssa,child_ssa\n202600001,202600002\n");
    REQUIRE(port.importDerivations(requestFor(source)).status ==
            ssa::ports::WorkflowStatus::Succeeded);
}

TEST_CASE("derivadas reader cancels a single large delimited line") {
    const Fixture fixture;
    const auto source = fixture.path("single-large-line.csv");
    std::string content = "parent_ssa,numero_ssa\n";
    content.append(32U * 1024U * 1024U, 'x');
    content += ",202600002\n";
    writeText(source, content);
    std::stop_source stopSource;
    std::latch started{1};
    auto operation = std::async(std::launch::async, [&] {
        started.count_down();
        return ssa::infra::importing::DerivadasSourceReader::read(source, stopSource.get_token());
    });

    started.wait();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{10}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{500}) == std::future_status::ready);
    REQUIRE(operation.get().status == ssa::infra::importing::DerivadasSourceStatus::Canceled);
}

TEST_CASE("derivadas edge merger cancels while aggregating a large batch") {
    std::vector<ssa::infra::importing::DerivationEdge> edges;
    edges.reserve(200'000);
    for (std::size_t index = 0; index < 200'000; ++index) {
        edges.push_back({"1" + std::to_string(index), "2" + std::to_string(index)});
    }
    ssa::infra::importing::DerivadasEdgeMerger merger;
    std::stop_source stopSource;
    std::latch started{1};
    auto operation = std::async(std::launch::async, [&] {
        started.count_down();
        return merger.add(edges, stopSource.get_token());
    });

    started.wait();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{10}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{500}) == std::future_status::ready);
    REQUIRE(operation.get().status == ssa::infra::importing::DerivadasMergeStatus::Canceled);
    REQUIRE(merger.parentByChild().size() < edges.size());
}

TEST_CASE("derivadas import cancels while waiting for SQLite and remains reusable") {
    const Fixture fixture;
    const auto source = fixture.path("locked.csv");
    writeText(source, "parent_ssa,child_ssa\n202600001,202600002\n");
    ssa::infra::sqlite::SqliteConnection blocker(fixture.databasePath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath,
                                                 unavailableLegacyConverter());
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return port.importDerivations(requestFor(source), stopSource.get_token());
    });

    REQUIRE(operation.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{500}) == std::future_status::ready);
    REQUIRE(operation.get().status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(fixture.parentOf("202600002").empty());
    REQUIRE(port.importDerivations(requestFor(source)).status ==
            ssa::ports::WorkflowStatus::Succeeded);
}

TEST_CASE("derivadas legacy XLS import exposes converter preflight") {
    const Fixture fixture;
    const auto source = fixture.path("legacy.xls");
    writeText(source, "legacy fixture");
    auto converter = std::make_shared<ssa::infra::importing::LegacySpreadsheetConverter>(
        fixture.path("missing-soffice"), nullptr);
    ssa::infra::sqlite::SqliteDerivadasPort port(fixture.databasePath, converter);

    REQUIRE_FALSE(port.legacySpreadsheetConverterAvailable());
    const auto result = port.importDerivations(requestFor(source));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("LibreOffice") != std::string::npos);
    REQUIRE(fixture.parentOf("202600001").empty());
}
