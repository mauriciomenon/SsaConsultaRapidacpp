#include "infra/import/XlsxWorkbookReader.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <miniz.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    void addEntry(mz_zip_archive& zip, const char* path, const std::string& content) {
        REQUIRE(mz_zip_writer_add_mem(&zip, path, content.data(), content.size(),
                                      MZ_BEST_COMPRESSION) != 0);
    }

    void writeWorkbook(const std::filesystem::path& path, const std::string& workbookProperties,
                       const std::string& styles, const std::string& cells,
                       const bool completeRows = false) {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0);
        addEntry(zip, "[Content_Types].xml", R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
</Types>)xml");
        addEntry(zip, "xl/workbook.xml",
                 R"xml(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)xml" +
                     workbookProperties +
                     R"xml(<sheets><sheet name="Data" sheetId="1" r:id="rId1"/></sheets>
</workbook>)xml");
        addEntry(zip, "xl/_rels/workbook.xml.rels",
                 R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
</Relationships>)xml");
        addEntry(zip, "xl/styles.xml", styles);
        const auto sheetRows = completeRows ? cells : "<row r=\"1\">" + cells + "</row>";
        addEntry(zip, "xl/worksheets/sheet1.xml",
                 R"xml(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<sheetData>)xml" + sheetRows +
                     R"xml(</sheetData></worksheet>)xml");
        REQUIRE(mz_zip_writer_finalize_archive(&zip) != 0);
        REQUIRE(mz_zip_writer_end(&zip) != 0);
    }

    void writeMultiSheetWorkbook(const std::filesystem::path& path,
                                 const std::vector<std::string>& sheets,
                                 const std::vector<std::string>& relationshipIds = {}) {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0);
        addEntry(zip, "[Content_Types].xml", R"xml(<?xml version="1.0" encoding="UTF-8"?>
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
            const auto relationshipId =
                relationshipIds.empty() ? "rId" + number : relationshipIds.at(index);
            workbook += "<sheet name=\"Sheet" + number + "\" sheetId=\"" + number + "\"";
            if (!relationshipId.empty()) {
                workbook += " r:id=\"" + relationshipId + "\"";
                relationships += "<Relationship Id=\"" + relationshipId +
                                 "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
                                 "relationships/worksheet\" Target=\"worksheets/sheet" +
                                 number + ".xml\"/>";
            }
            workbook += "/>";
            addEntry(zip, ("xl/worksheets/sheet" + number + ".xml").c_str(),
                     R"xml(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1">)xml" +
                         sheets[index] + "</row></sheetData></worksheet>");
        }
        workbook += "</sheets></workbook>";
        relationships += "</Relationships>";
        addEntry(zip, "xl/workbook.xml", workbook);
        addEntry(zip, "xl/_rels/workbook.xml.rels", relationships);
        REQUIRE(mz_zip_writer_finalize_archive(&zip) != 0);
        REQUIRE(mz_zip_writer_end(&zip) != 0);
    }

    const std::string builtInDateStyles = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<cellXfs count="2"><xf numFmtId="0"/><xf numFmtId="14" applyNumberFormat="1"/></cellXfs>
</styleSheet>)xml";

} // namespace

TEST_CASE("xlsx reader converts Excel 1900 date cells and corrects the leap-day bug") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "dates.xlsx";
    writeWorkbook(path, {}, builtInDateStyles,
                  R"xml(<c r="A1" s="1"><v>60</v></c><c r="B1" s="1"><v>61.5</v></c>)xml");

    const auto table = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path);

    REQUIRE(table.rows.at(0).at(0) == "1900-02-28T00:00:00");
    REQUIRE(table.rows.at(0).at(1) == "1900-03-01T12:00:00");
}

TEST_CASE("xlsx reader bounds date serials to the Excel calendar") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto lastDay = root / "last-day.xlsx";
    writeWorkbook(lastDay, {}, builtInDateStyles, R"xml(<c r="A1" s="1"><v>2958465</v></c>)xml");
    REQUIRE(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(lastDay).rows.at(0).at(0) ==
            "9999-12-31T00:00:00");

    // Past 9999-12-31, and far past it (formerly an out-of-range cast to qint64).
    for (const auto* serial : {"2958466", "1e20"}) {
        INFO(serial);
        const auto path = root / (std::string{"serial-"} + serial + ".xlsx");
        writeWorkbook(path, {}, builtInDateStyles,
                      std::string{R"xml(<c r="A1" s="1"><v>)xml"} + serial + "</v></c>");
        REQUIRE_THROWS_WITH(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path),
                            Catch::Matchers::ContainsSubstring("out of supported range"));
    }
}

TEST_CASE("xlsx reader honors date1904 for custom date formats") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "date1904.xlsx";
    const std::string styles = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<numFmts count="1"><numFmt numFmtId="164" formatCode="yyyy-mm-dd hh:mm:ss"/></numFmts>
<cellXfs count="2"><xf numFmtId="0"/><xf numFmtId="164" applyNumberFormat="1"/></cellXfs>
</styleSheet>)xml";
    writeWorkbook(path, R"xml(<workbookPr date1904="1"/>)xml", styles,
                  R"xml(<c r="A1" s="1"><v>0.25</v></c>)xml");

    const auto table = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path);

    REQUIRE(table.rows.at(0).at(0) == "1904-01-01T06:00:00");
}

TEST_CASE("xlsx reader rejects formulas without a cached value") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "formula.xlsx";
    writeWorkbook(path, {}, builtInDateStyles, R"xml(<c r="A1"><f>NOW()</f></c>)xml");

    bool rejected = false;
    try {
        static_cast<void>(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path));
    } catch (const std::runtime_error& error) {
        rejected = true;
        REQUIRE(std::string{error.what()} == "xlsx formula cell has no cached value");
    }
    REQUIRE(rejected);
}

TEST_CASE("xlsx reader uses a cached value for formula date cells") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "cached-formula.xlsx";
    writeWorkbook(path, {}, builtInDateStyles,
                  R"xml(<c r="A1" s="1"><f>DATE(2026,7,14)</f><v>46217</v></c>)xml");

    const auto table = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path);

    REQUIRE(table.rows.at(0).at(0) == "2026-07-14T00:00:00");
}

TEST_CASE("xlsx reader preserves a blank cell that only has a date style") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "blank-date.xlsx";
    writeWorkbook(path, {}, builtInDateStyles, R"xml(<c r="A1" s="1"/>)xml");

    const auto table = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path);

    REQUIRE(table.rows.at(0).at(0).empty());
}

TEST_CASE("xlsx reader preserves Python raw scalar types separately from display text") {
    using ssa::infra::importing::RawCell;
    using ssa::infra::importing::XlsxWorkbookReader;
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "raw-types.xlsx";
    writeWorkbook(path, {}, builtInDateStyles,
                  R"xml(<c r="A1" t="inlineStr"><is><t xml:space="preserve"> text </t></is></c>
<c r="B1"><v>002</v></c><c r="C1"><v>2.50</v></c>
<c r="D1" t="b"><v>1</v></c><c r="E1" t="b"><v>0</v></c>
<c r="F1" s="1"><v>46217</v></c><c r="G1" t="d"><v>2026-07-13</v></c>
<c r="H1" t="d"><v>2026-07-13T08:15:00.123</v></c>
<c r="I1" s="1"><v>0.34375</v></c><c r="K1" s="1"/>
<c r="L1"><v>1e6</v></c><c r="M1"><v>1e-5</v></c>)xml");
    const auto table = XlsxWorkbookReader::readFirstSheet(path);
    const auto& raw = table.rawRows.at(0);
    const std::vector<RawCell> expected{{RawCell::Kind::String, " text "},
                                        {RawCell::Kind::Number, "2"},
                                        {RawCell::Kind::Number, "2.5"},
                                        {RawCell::Kind::Boolean, "True"},
                                        {RawCell::Kind::Boolean, "False"},
                                        {RawCell::Kind::DateTime, "2026-07-14 00:00:00"},
                                        {RawCell::Kind::Date, "2026-07-13"},
                                        {RawCell::Kind::DateTime, "2026-07-13 08:15:00.123000"},
                                        {RawCell::Kind::Time, "08:15:00"},
                                        // Omitted J1 and empty K1 sit inside the sheet: "" as
                                        // pandas reads them with na_filter=False.
                                        {RawCell::Kind::String, ""},
                                        {RawCell::Kind::String, ""},
                                        {RawCell::Kind::Number, "1000000.0"},
                                        {RawCell::Kind::Number, "1e-05"}};
    REQUIRE(raw.size() == expected.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        INFO(index);
        REQUIRE(raw[index].kind == expected[index].kind);
        REQUIRE(raw[index].value == expected[index].value);
    }
    REQUIRE(table.rows.at(0).at(1) == "002");
    REQUIRE(table.rows.at(0).at(3) == "1");
    REQUIRE(table.rows.at(0).at(5) == "2026-07-14T00:00:00");
}

TEST_CASE("xlsx raw cells stay aligned across sparse rows and chunk boundaries") {
    using ssa::infra::importing::RawCell;
    using ssa::infra::importing::SpreadsheetTable;
    using ssa::infra::importing::XlsxWorkbookReader;
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "raw-chunks.xlsx";
    writeWorkbook(path, {}, builtInDateStyles,
                  R"xml(<row r="1"><c r="B1" t="b"><v>1</v></c></row>
<row r="3"><c r="A3"><v>2.5</v></c></row>)xml",
                  true);
    std::vector<SpreadsheetTable> chunks;
    XlsxWorkbookReader::readSheetChunks(path, 1,
                                        [&](SpreadsheetTable chunk, const bool, const bool) {
                                            REQUIRE(chunk.rows.size() == chunk.rawRows.size());
                                            chunks.push_back(std::move(chunk));
                                        });
    REQUIRE(chunks.size() == 3);
    REQUIRE(chunks[0].rawRows[0][0].kind == RawCell::Kind::String);
    REQUIRE(chunks[0].rawRows[0][0].value.empty());
    REQUIRE(chunks[0].rawRows[0][1].value == "True");
    REQUIRE(chunks[1].rawRows[0].empty());
    REQUIRE(chunks[2].rawRows[0][0].kind == RawCell::Kind::Number);
    REQUIRE(chunks[2].rawRows[0][0].value == "2.5");
}

TEST_CASE("xlsx raw numbers reject nonfinite and invalid JSON scalars") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "invalid-number.xlsx";
    for (const auto* number : {"NaN", "Infinity", "1e999", "-"}) {
        INFO(number);
        writeWorkbook(path, {}, builtInDateStyles,
                      std::string{"<c r=\"A1\"><v>"} + number + "</v></c>");
        REQUIRE_THROWS_WITH(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path),
                            "invalid xlsx original number");
    }
}

TEST_CASE("xlsx original time cells use custom formats and millisecond precision") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "raw-time.xlsx";
    const std::string styles =
        R"xml(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<numFmts count="1"><numFmt numFmtId="164" formatCode="hh:mm:ss.000"/></numFmts>
<cellXfs count="2"><xf numFmtId="0"/><xf numFmtId="164"/></cellXfs></styleSheet>)xml";
    writeWorkbook(path, R"xml(<workbookPr date1904="1"/>)xml", styles,
                  R"xml(<c r="A1" s="1"><v>0.3437514236111111</v></c>
<c r="B1" t="d"><v>08:15:00.123</v></c>)xml");
    const auto table = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(path);
    for (const auto& raw : table.rawRows.at(0)) {
        REQUIRE(raw.kind == ssa::infra::importing::RawCell::Kind::Time);
        REQUIRE(raw.value == "08:15:00.123000");
    }
}

TEST_CASE("xlsx reader returns every worksheet in workbook order") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "sheets.xlsx";
    writeMultiSheetWorkbook(path, {R"xml(<c r="A1" t="inlineStr"><is><t>Cover</t></is></c>)xml",
                                   R"xml(<c r="A1" t="inlineStr"><is><t>Data</t></is></c>)xml"});

    const auto sheets = ssa::infra::importing::XlsxWorkbookReader::readSheets(path);

    REQUIRE(sheets.size() == 2);
    REQUIRE(sheets.at(0).rows.at(0).at(0) == "Cover");
    REQUIRE(sheets.at(1).rows.at(0).at(0) == "Data");
}

TEST_CASE("xlsx reader streams worksheet rows in bounded ordered chunks") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "chunks.xlsx";
    std::string rows;
    for (std::size_t index = 0; index < 2'005; ++index) {
        rows += "<row r=\"" + std::to_string(index + 1) + "\"><c r=\"A" +
                std::to_string(index + 1) + "\"><v>" + std::to_string(index) + "</v></c></row>";
    }
    writeWorkbook(path, {}, builtInDateStyles, rows, true);
    std::vector<std::size_t> chunkSizes;
    std::vector<std::string> values;

    ssa::infra::importing::XlsxWorkbookReader::readSheetChunks(
        path, 1'000,
        [&](ssa::infra::importing::SpreadsheetTable chunk, const bool firstInSheet,
            const bool lastInSheet) {
            REQUIRE(firstInSheet == chunkSizes.empty());
            REQUIRE(lastInSheet == (chunkSizes.size() == 2));
            chunkSizes.push_back(chunk.rows.size());
            for (const auto& row : chunk.rows) {
                values.push_back(row.at(0));
            }
        });

    REQUIRE(chunkSizes == std::vector<std::size_t>{1'000, 1'000, 5});
    REQUIRE(values.size() == 2'005);
    REQUIRE(values.front() == "0");
    REQUIRE(values.back() == "2004");
}

TEST_CASE("xlsx reader streams a worksheet XML entry larger than the buffered entry limit") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "large-entry.xlsx";
    constexpr std::size_t bufferedEntryLimit = 32ULL * 1024ULL * 1024ULL;
    const std::string rows =
        std::string(bufferedEntryLimit + 1, ' ') + "<row r=\"1\"><c r=\"A1\"><v>7</v></c></row>";
    writeWorkbook(path, {}, builtInDateStyles, rows, true);
    std::size_t rowCount = 0;

    ssa::infra::importing::XlsxWorkbookReader::readSheetChunks(
        path, 1'000, [&](ssa::infra::importing::SpreadsheetTable chunk, const bool, const bool) {
            rowCount += chunk.rows.size();
        });

    REQUIRE(rowCount == 1);
}

TEST_CASE("xlsx reader rejects a formula without cache in any worksheet") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = std::filesystem::path{tempDir.path().toStdString()} / "formula-sheet2.xlsx";
    writeMultiSheetWorkbook(path, {R"xml(<c r="A1" t="inlineStr"><is><t>cover</t></is></c>)xml",
                                   R"xml(<c r="A1"><f>NOW()</f></c>)xml"});

    bool rejected = false;
    try {
        static_cast<void>(ssa::infra::importing::XlsxWorkbookReader::readSheets(path));
    } catch (const std::runtime_error& error) {
        rejected = true;
        REQUIRE(std::string{error.what()} == "xlsx formula cell has no cached value");
    }
    REQUIRE(rejected);
}

TEST_CASE("xlsx reader rejects missing and duplicate worksheet relationship ids") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto missing = root / "missing-id.xlsx";
    const auto duplicate = root / "duplicate-id.xlsx";
    const std::vector<std::string> sheets{
        R"xml(<c r="A1" t="inlineStr"><is><t>First</t></is></c>)xml",
        R"xml(<c r="A1" t="inlineStr"><is><t>Second</t></is></c>)xml"};
    writeMultiSheetWorkbook(missing, sheets, {"rId1", ""});
    writeMultiSheetWorkbook(duplicate, sheets, {"rId1", "rId1"});

    REQUIRE_THROWS_AS(ssa::infra::importing::XlsxWorkbookReader::readSheets(missing),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ssa::infra::importing::XlsxWorkbookReader::readSheets(duplicate),
                      std::runtime_error);
}
