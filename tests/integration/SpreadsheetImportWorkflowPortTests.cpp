#include "domain/ColumnCatalog.h"
#include "infra/import/ImportFileStager.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <miniz.h>
#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

    std::vector<ssa::domain::ColumnDef> importColumns() {
        const auto columns = ssa::domain::ColumnCatalog::all();
        return {columns.begin(), columns.end()};
    }

    void addZipEntry(mz_zip_archive& zip, const char* path, const std::string& content) {
        REQUIRE(mz_zip_writer_add_mem(&zip, path, content.data(), content.size(),
                                      MZ_BEST_COMPRESSION) != 0);
    }

    void writeWorkbook(const std::filesystem::path& path, const std::string& rowsXml) {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0);
        addZipEntry(zip, "[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
</Types>)");
        addZipEntry(zip, "xl/workbook.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets><sheet name="Consulta SSA" sheetId="1" r:id="rId1"/></sheets>
</workbook>)");
        addZipEntry(zip, "xl/_rels/workbook.xml.rels", R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
</Relationships>)");
        addZipEntry(zip, "xl/worksheets/sheet1.xml",
                    R"(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<dimension ref="A1:E4"/><sheetData>)" +
                        rowsXml + "</sheetData></worksheet>");
        REQUIRE(mz_zip_writer_finalize_archive(&zip) != 0);
        REQUIRE(mz_zip_writer_end(&zip) != 0);
    }

    std::string inlineCell(const char* ref, const std::string& value) {
        return std::string{"<c r=\""} + ref + "\" t=\"inlineStr\"><is><t>" + value +
               "</t></is></c>";
    }

    std::string row(const int index, const std::vector<std::string>& cells) {
        std::string xml = "<row r=\"" + std::to_string(index) + "\">";
        for (const auto& cell : cells) {
            xml += cell;
        }
        xml += "</row>";
        return xml;
    }

    int scalarInt(sqlite3* db, const char* sql) {
        sqlite3_stmt* statement = nullptr;
        REQUIRE(sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
        const int value = sqlite3_column_int(statement, 0);
        REQUIRE(sqlite3_finalize(statement) == SQLITE_OK);
        return value;
    }

    std::string scalarText(sqlite3* db, const char* sql) {
        sqlite3_stmt* statement = nullptr;
        REQUIRE(sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        std::string value = text == nullptr ? std::string{} : std::string{text};
        REQUIRE(sqlite3_finalize(statement) == SQLITE_OK);
        return value;
    }

#ifndef _WIN32
    std::filesystem::path writeFakeSoffice(const std::filesystem::path& directory) {
        const auto executable = directory / "fake-soffice";
        std::ofstream script(executable);
        script << R"SH(#!/bin/sh
outdir=""
source=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --outdir)
            shift
            outdir="$1"
            ;;
        --convert-to)
            shift
            ;;
        --headless)
            ;;
        *)
            source="$1"
            ;;
    esac
    shift
done
base="$(basename "$source")"
stem="${base%.*}"
cp "$source" "$outdir/$stem.xlsx"
)SH";
        script.close();
        std::filesystem::permissions(executable,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add);
        return executable;
    }
#endif

} // namespace

TEST_CASE("spreadsheet import workflow stages xlsx and writes sqlite rows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto workbook = sourceDirectory / "entrada.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Setor Executor"), inlineCell("D1", "Descricao da SSA")}) +
            row(2, {inlineCell("A2", "202600001"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "MEL1"), inlineCell("D2", "Primeira importacao")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {workbook};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.message.find("rows=1") != std::string::npos);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarText(db, "SELECT setor_executor FROM ssa_table WHERE numero_ssa='202600001'") ==
            "MEL1");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet import workflow maps sparse xlsx cells by cell reference") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto workbook = sourceDirectory / "sparse.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA"),
                                    inlineCell("D1", "Setor Executor")}) +
                                row(2, {inlineCell("A2", "202600777"), inlineCell("D2", "MEL9")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {workbook};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT setor_executor FROM ssa_table WHERE numero_ssa='202600777'") ==
            "MEL9");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet import workflow reports xlsx read failure cause") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto workbook = sourceDirectory / "broken.xlsx";
    std::ofstream output(workbook);
    output << "not a zip package";
    output.close();

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {workbook};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("failed=1") != std::string::npos);
    REQUIRE(result.message.find("error=cannot read xlsx zip package") != std::string::npos);
}

TEST_CASE("spreadsheet import workflow full rescan replaces existing rows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto workbook = inputDirectory / "first.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600010"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Linha nova")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    REQUIRE(port.rescan({ssa::ports::RescanMode::Full}).ok());
    REQUIRE(port.rescan({ssa::ports::RescanMode::Full}).ok());

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet import workflow reports empty full rescan write failure") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "missing-parent" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::RescanRequest request;
    request.mode = ssa::ports::RescanMode::Full;

    ssa::ports::WorkflowResult result;
    REQUIRE_NOTHROW(result = port.rescan(request));

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("error=") != std::string::npos);
}

TEST_CASE("spreadsheet import workflow reports legacy xls when converter is unavailable") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto legacy = sourceDirectory / "legacy.xls";
    std::ofstream output(legacy);
    output << "legacy";
    output.close();

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {legacy};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("legacy_xls=1") != std::string::npos);
}

TEST_CASE("import file stager counts legacy xls with existing xlsx without conversion") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory = std::filesystem::path{tempDir.path().toStdString()};
    const auto legacy = inputDirectory / "legacy.xls";
    const auto workbook = inputDirectory / "legacy.xlsx";
    {
        std::ofstream output(legacy);
        output << "legacy";
    }
    {
        std::ofstream output(workbook);
        output << "already converted";
    }

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);

    const auto result = stager.stageInputFiles();

    REQUIRE(result.legacyXls == 1);
    REQUIRE(result.convertedXls == 0);
    REQUIRE(result.failedLegacyXls == 0);
    REQUIRE(result.unsupported == 0);
    REQUIRE(result.xlsxFiles == std::vector<std::filesystem::path>{workbook});
}

TEST_CASE("import file stager reports unreadable input folder scan") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputPath = std::filesystem::path{tempDir.path().toStdString()} / "not-a-folder";
    {
        std::ofstream output(inputPath);
        output << "not a directory";
    }

    const ssa::infra::importing::ImportFileStager stager(inputPath);

    const auto result = stager.stageInputFiles();

    REQUIRE(result.failedCopies == 1);
    REQUIRE(result.xlsxFiles.empty());
}

#ifndef _WIN32
TEST_CASE("spreadsheet import workflow converts staged xls before sqlite import") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());

    const auto legacyWorkbook = sourceDirectory / "legacy.xls";
    writeWorkbook(
        legacyWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Setor Executor"), inlineCell("D1", "Descricao da SSA")}) +
            row(2, {inlineCell("A2", "202600099"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "MEL2"), inlineCell("D2", "Legado convertido")}));

    const auto fakeSoffice = writeFakeSoffice(root);
    ssa::infra::importing::LegacySpreadsheetConverter converter(fakeSoffice);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns(), converter);
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {legacyWorkbook};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.message.find("converted_xls=1") != std::string::npos);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarText(db, "SELECT setor_executor FROM ssa_table WHERE numero_ssa='202600099'") ==
            "MEL2");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}
#endif
