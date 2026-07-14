#include "domain/ColumnCatalog.h"
#include "infra/import/ImportFileStager.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/import/XlsxPackage.h"
#include "infra/import/XlsxWorkbookReader.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "qt/FilesystemPath.h"

#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <miniz.h>
#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <latch>
#include <stop_token>
#include <string>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

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
        mz_zip_archive zip = {};
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

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    std::size_t directWorkbookCount(const std::filesystem::path& directory) {
        std::error_code error;
        std::size_t count = 0;
        for (std::filesystem::directory_iterator iterator(directory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            const auto extension = iterator->path().extension();
            if (iterator->is_regular_file(error) && (extension == ".xls" || extension == ".xlsx")) {
                ++count;
            }
            error.clear();
        }
        REQUIRE_FALSE(error);
        return count;
    }

    void createSparseFile(const std::filesystem::path& path, const std::uintmax_t size) {
        std::ofstream output(path, std::ios::binary);
        REQUIRE(output.good());
        output.close();
        std::filesystem::resize_file(path, size);
    }

#ifndef _WIN32
    enum class FakeSofficeBehavior { Copy, Block, CopyThenBlock, CleanupFailure };

    std::filesystem::path
    writeFakeSoffice(const std::filesystem::path& directory,
                     const FakeSofficeBehavior behavior = FakeSofficeBehavior::Copy) {
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
)SH";
        switch (behavior) {
        case FakeSofficeBehavior::Copy:
            script << "cp \"$source\" \"$outdir/$stem.xlsx\"\n";
            break;
        case FakeSofficeBehavior::Block:
            script << "printf 'partial' > \"$outdir/$stem.xlsx\"\n"
                      "while :; do sleep 1; done\n";
            break;
        case FakeSofficeBehavior::CopyThenBlock:
            script << "if [ \"$stem\" = a ]; then\n"
                      "    cp \"$source\" \"$outdir/$stem.xlsx\"\n"
                      "else\n"
                      "    printf 'partial' > \"$outdir/$stem.xlsx\"\n"
                      "    while :; do sleep 1; done\n"
                      "fi\n";
            break;
        case FakeSofficeBehavior::CleanupFailure:
            script << "cp \"$source\" \"$outdir/$stem.xlsx\"\n"
                      "chmod 500 \"$outdir\"\n";
            break;
        }
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

TEST_CASE("spreadsheet import workflow rejects a stopped token before staging") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {root / "source.xlsx"};
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = port.importExternalFiles(request, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("import file stager rejects a stopped token before copying") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = stager.stageExternalFiles({root / "source.xlsx"}, stopSource.get_token());

    REQUIRE(result.rejectionReason == "canceled");
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
}

TEST_CASE("import file stager cancels a copy without publishing partial files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "large-source.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    createSparseFile(source, copyBytes);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    std::stop_source stopSource;

    auto future = std::async(std::launch::async, [&] {
        return stager.stageExternalFiles({source}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporary = false;
    while (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(inputDirectory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") != std::string::npos &&
                iterator->file_size(error) > 0 && !error) {
                observedTemporary = true;
                stopSource.request_stop();
                break;
            }
        }
        QThread::msleep(1);
    }
    stopSource.request_stop();
    const auto result = future.get();

    REQUIRE(observedTemporary);
    REQUIRE(result.rejectionReason == "canceled");
    REQUIRE(result.files.empty());
    REQUIRE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::is_empty(inputDirectory));
}

TEST_CASE("staging preserves prior diagnostics without misclassifying clean cancellation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto legacy = root / "broken.xls";
    const auto largeSource = root / "large-source.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    createSparseFile(legacy, 1);
    createSparseFile(largeSource, copyBytes);
    const ssa::infra::importing::ImportFileStager stager(
        inputDirectory,
        ssa::infra::importing::LegacySpreadsheetConverter(root / "missing-soffice"));
    std::stop_source stopSource;
    auto future = std::async(std::launch::async, [&] {
        return stager.stageExternalFiles({legacy, largeSource}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    while (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(inputDirectory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") != std::string::npos &&
                iterator->file_size(error) > 0 && !error) {
                stopSource.request_stop();
                break;
            }
        }
        QThread::msleep(1);
    }
    stopSource.request_stop();
    const auto result = future.get();

    REQUIRE(result.rejectionReason == "canceled");
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(result.files.empty());
    REQUIRE(std::filesystem::is_empty(inputDirectory));
}

TEST_CASE("workflow cancellation after staging removes the owned external copy") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "selected.xlsx";
    writeWorkbook(source, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600408"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Cancelar apos staging")}));
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600409"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600409");
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return port.importExternalFiles({.files = {source}}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    while ((!std::filesystem::exists(inputDirectory) || directWorkbookCount(inputDirectory) == 0) &&
           deadline.elapsed() < 3'000) {
        QThread::msleep(5);
    }
    REQUIRE(directWorkbookCount(inputDirectory) == 1);
    REQUIRE(operation.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);

    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    const auto result = operation.get();
    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(directWorkbookCount(inputDirectory) == 0);
    REQUIRE(std::filesystem::exists(source));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600409'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600408'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

#ifndef _WIN32
TEST_CASE("workflow reports failed when owned staging cleanup cannot complete after cancellation") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "selected.xlsx";
    writeWorkbook(source, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600410"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Cleanup bloqueado")}));
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write({}, 0, 0, false).rowsWritten == 0);
    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return port.importExternalFiles({.files = {source}}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    while ((!std::filesystem::exists(inputDirectory) || directWorkbookCount(inputDirectory) == 0) &&
           deadline.elapsed() < 3'000) {
        QThread::msleep(5);
    }
    REQUIRE(directWorkbookCount(inputDirectory) == 1);
    std::filesystem::permissions(
        inputDirectory, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    const auto result = operation.get();
    std::filesystem::permissions(inputDirectory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("staging_cleanup_failed") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(directWorkbookCount(inputDirectory) == 1);
    REQUIRE(std::filesystem::exists(source));
}

TEST_CASE("staging cleanup failure is not masked as canceled") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "large-source.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    createSparseFile(source, copyBytes);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return stager.stageExternalFiles({source}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporary = false;
    while (operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(inputDirectory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") != std::string::npos &&
                iterator->file_size(error) > 0 && !error) {
                observedTemporary = true;
                break;
            }
        }
        if (observedTemporary) {
            break;
        }
        QThread::msleep(1);
    }
    REQUIRE(observedTemporary);
    std::filesystem::permissions(
        inputDirectory, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    const auto result = operation.get();
    std::filesystem::permissions(inputDirectory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    REQUIRE(result.rejectionReason == "staging_cleanup_failed");
    REQUIRE(result.diagnostic.find("cannot remove staged temporary file") != std::string::npos);
}
#endif

#ifndef _WIN32
TEST_CASE("import file stager rejects a symlinked input directory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto realInputDirectory = root / "real-input";
    const auto inputDirectory = root / "docs_entrada";
    const auto source = root / "source.xlsx";
    std::filesystem::create_directories(realInputDirectory);
    createSparseFile(source, 1);
    std::filesystem::create_directory_symlink(realInputDirectory, inputDirectory);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);

    SECTION("external staging preserves its source") {
        const auto result = stager.stageExternalFiles({source});

        REQUIRE(result.rejectionReason == "input_directory_symlink");
        REQUIRE(result.files.empty());
        REQUIRE(std::filesystem::exists(source));
        REQUIRE(std::filesystem::is_empty(realInputDirectory));
    }

    SECTION("input discovery does not traverse the target") {
        const auto workbook = realInputDirectory / "inside.xlsx";
        createSparseFile(workbook, 1);

        const auto result = stager.stageInputFiles();

        REQUIRE(result.rejectionReason == "input_directory_symlink");
        REQUIRE(result.files.empty());
        REQUIRE(std::filesystem::exists(workbook));
    }

    SECTION("consolidation preserves its source") {
        const std::vector<ssa::infra::importing::ImportManifestEntry> manifest{{{source}, true}};

        const auto result = stager.consolidate(manifest);

        REQUIRE(result.failed == 1);
        REQUIRE(result.error == "input_directory_symlink");
        REQUIRE(std::filesystem::exists(source));
        REQUIRE(std::filesystem::is_empty(realInputDirectory));
    }
}
#endif

TEST_CASE("import file consolidation observes cancellation with an empty source list") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const std::vector<ssa::infra::importing::ImportManifestEntry> manifest{{{}, true}};
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = stager.consolidate(manifest, stopSource.get_token());

    REQUIRE(result.canceled);
    REQUIRE(result.error == "consolidation canceled");
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
}

TEST_CASE("legacy converter rejects a stopped token before starting a process") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const ssa::infra::importing::LegacySpreadsheetConverter converter(root / "soffice");
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = converter.convertToXlsx({root / "source.xls", root / "output.xlsx"},
                                                stopSource.get_token());

    REQUIRE(result.status == ssa::infra::importing::LegacySpreadsheetConversionStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(root / "output.xlsx"));
}

#ifndef _WIN32
TEST_CASE("legacy converter cancellation preserves destination and removes temporary output") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "source.xls";
    const auto destination = root / "output.xlsx";
    {
        std::ofstream input(source);
        input << "source";
        std::ofstream previous(destination);
        previous << "previous";
    }
    const ssa::infra::importing::LegacySpreadsheetConverter converter(
        writeFakeSoffice(root, FakeSofficeBehavior::Block));
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return converter.convertToXlsx({source, destination}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporaryOutput = false;
    while (operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().parent_path().filename().string().starts_with(
                    "ssa_xls_conversion_") &&
                iterator->path().extension() == ".xlsx") {
                observedTemporaryOutput = true;
                stopSource.request_stop();
                break;
            }
        }
        QThread::msleep(5);
    }
    stopSource.request_stop();
    const auto result = operation.get();

    REQUIRE(observedTemporaryOutput);
    REQUIRE(result.status == ssa::infra::importing::LegacySpreadsheetConversionStatus::Canceled);
    REQUIRE(readFile(destination) == "previous");
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        REQUIRE_FALSE(entry.path().filename().string().starts_with("ssa_xls_conversion_"));
    }
}
#endif

#ifndef _WIN32
TEST_CASE("input staging cancellation removes only converted artifacts owned by the operation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    const auto firstLegacy = inputDirectory / "a.xls";
    const auto blockingLegacy = inputDirectory / "block.xls";
    const auto preservedWorkbook = inputDirectory / "0_keep.xlsx";
    writeWorkbook(firstLegacy, row(1, {inlineCell("A1", "Numero SSA")}));
    writeWorkbook(blockingLegacy, row(1, {inlineCell("A1", "Numero SSA")}));
    writeWorkbook(preservedWorkbook, row(1, {inlineCell("A1", "Numero SSA")}));
    const ssa::infra::importing::ImportFileStager stager(
        inputDirectory, ssa::infra::importing::LegacySpreadsheetConverter(
                            writeFakeSoffice(root, FakeSofficeBehavior::CopyThenBlock)));
    std::stop_source stopSource;

    auto operation = std::async(std::launch::async,
                                [&] { return stager.stageInputFiles(stopSource.get_token()); });
    QElapsedTimer deadline;
    deadline.start();
    const auto firstConverted = inputDirectory / "a.xlsx";
    bool observedBlockingTemporary = false;
    while (operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator iterator(inputDirectory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename() == "block.xlsx" &&
                iterator->path().parent_path().filename().string().starts_with(
                    "ssa_xls_conversion_")) {
                observedBlockingTemporary = true;
                break;
            }
        }
        if (observedBlockingTemporary) {
            break;
        }
        QThread::msleep(5);
    }
    REQUIRE(std::filesystem::exists(firstConverted));
    REQUIRE(observedBlockingTemporary);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::seconds{7}) == std::future_status::ready);
    const auto result = operation.get();

    REQUIRE(result.rejectionReason == "canceled");
    REQUIRE(result.files.empty());
    REQUIRE(std::filesystem::exists(firstLegacy));
    REQUIRE(std::filesystem::exists(blockingLegacy));
    REQUIRE(std::filesystem::exists(preservedWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(firstConverted));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "block.xlsx"));
    for (const auto& entry : std::filesystem::directory_iterator(inputDirectory)) {
        REQUIRE_FALSE(entry.path().filename().string().starts_with("ssa_xls_conversion_"));
    }
}

TEST_CASE("published legacy conversion remains successful when temporary cleanup fails") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto legacy = sourceDirectory / "cleanup.xls";
    writeWorkbook(legacy, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600407"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Cleanup warning")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(
        inputDirectory, dbPath, importColumns(),
        ssa::infra::importing::LegacySpreadsheetConverter(
            writeFakeSoffice(root, FakeSofficeBehavior::CleanupFailure)));

    const auto result = port.importExternalFiles({.files = {legacy}});

    std::size_t temporaryDirectories = 0;
    for (const auto& entry : std::filesystem::directory_iterator(inputDirectory)) {
        if (entry.is_directory() &&
            entry.path().filename().string().starts_with("ssa_xls_conversion_")) {
            ++temporaryDirectories;
            std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
            REQUIRE(std::filesystem::remove_all(entry.path()) > 0);
        }
    }
    REQUIRE(temporaryDirectories == 1);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.diagnostic.find("cannot remove xls conversion temporary directory") !=
            std::string::npos);
    REQUIRE(std::filesystem::exists(legacy));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600407'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("cleanup warning before commit is preserved when full rescan rejects the workbook") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto legacy = inputDirectory / "cleanup.xls";
    writeWorkbook(legacy, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}));
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600411"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600411");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(
        inputDirectory, dbPath, importColumns(),
        ssa::infra::importing::LegacySpreadsheetConverter(
            writeFakeSoffice(root, FakeSofficeBehavior::CleanupFailure)));

    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    std::size_t temporaryDirectories = 0;
    for (const auto& entry : std::filesystem::directory_iterator(inputDirectory)) {
        if (entry.is_directory() &&
            entry.path().filename().string().starts_with("ssa_xls_conversion_")) {
            ++temporaryDirectories;
            std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
            REQUIRE(std::filesystem::remove_all(entry.path()) > 0);
        }
    }
    REQUIRE(temporaryDirectories == 1);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("staging_cleanup_failed") != std::string::npos);
    REQUIRE(result.diagnostic.find("cannot remove xls conversion temporary directory") !=
            std::string::npos);
    REQUIRE(std::filesystem::exists(legacy));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "cleanup.xlsx"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600411'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}
#endif

TEST_CASE("xlsx reader rejects a stopped token before opening the package") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    std::stop_source stopSource;
    stopSource.request_stop();

    REQUIRE_THROWS_AS(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(
                          root / "missing.xlsx", stopSource.get_token()),
                      std::system_error);
}

TEST_CASE("xlsx extraction cancellation is prompt and a second read succeeds") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::size_t largeXmlBytes = std::size_t{24} * 1024 * 1024;
    const auto workbook =
        std::filesystem::path{tempDir.path().toStdString()} / "large-workbook.xlsx";
    const auto rowsXml = "<!--" + std::string(largeXmlBytes, 'x') + "-->" +
                         row(1, {inlineCell("A1", "Numero SSA")}) +
                         row(2, {inlineCell("A2", "202600300")});
    writeWorkbook(workbook, rowsXml);
    ssa::infra::importing::XlsxPackage package(workbook);
    std::stop_source stopSource;
    std::latch extractionStarted{1};
    auto operation = std::async(std::launch::async, [&] {
        try {
            extractionStarted.count_down();
            static_cast<void>(
                package.textEntry("xl/worksheets/sheet1.xml", true, stopSource.get_token()));
            return std::error_code{};
        } catch (const std::system_error& error) {
            return error.code();
        }
    });

    extractionStarted.wait();
    QThread::msleep(1);
    stopSource.request_stop();

    REQUIRE(operation.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    REQUIRE(operation.get() == std::make_error_code(std::errc::operation_canceled));
    const auto secondRead = ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(workbook);
    REQUIRE(secondRead.rows.size() == 2);
    REQUIRE(secondRead.rows.at(1).at(0) == "202600300");
}

TEST_CASE("sqlite import writer rejects a stopped token before creating the database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "data" / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    std::stop_source stopSource;
    stopSource.request_stop();

    REQUIRE_THROWS_AS(writer.startSession(false, stopSource.get_token()), std::system_error);
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("sqlite import writer rolls back schema when a session is abandoned") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(dbPath.parent_path());
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());

    {
        auto session = writer.startSession(false);
        session.rollback();
        session.rollback();
        REQUIRE_THROWS_AS(session.write({}, 0, 0), std::logic_error);
        REQUIRE_THROWS_AS(session.finish(), std::logic_error);
    }

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(
                db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='ssa_table'") ==
            0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import cancellation rolls back rows and permits a second write") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(dbPath.parent_path());
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600200"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600200");
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600201"}, {"descricao_ssa", "Nova"}});
    replacement.ssaNumbersForUpsertDelete.emplace_back("202600201");
    std::stop_source stopSource;
    {
        auto session = writer.startSession(true, stopSource.get_token());
        session.write(replacement, 1, 0);
        stopSource.request_stop();
        REQUIRE_THROWS_AS(session.finish(), std::system_error);
        REQUIRE_NOTHROW(session.rollback());
        REQUIRE_THROWS_AS(session.write(replacement, 1, 0), std::logic_error);
        REQUIRE_THROWS_AS(session.finish(), std::logic_error);
    }

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600200'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600201'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    REQUIRE(writer.write(replacement, 1, 0, true).rowsWritten == 1);
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600200'") == 0);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600201'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import stops promptly while locked and remains reusable") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600205"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600205");
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600206"}, {"descricao_ssa", "Nova"}});
    replacement.ssaNumbersForUpsertDelete.emplace_back("202600206");
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(writer.write(replacement, 1, 0, true, stopSource.get_token()));
            return std::error_code{};
        } catch (const std::system_error& error) {
            return error.code();
        }
    });

    REQUIRE(operation.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{500}) == std::future_status::ready);
    REQUIRE(operation.get() == std::make_error_code(std::errc::operation_canceled));
    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);

    sqlite3* verification = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &verification) == SQLITE_OK);
    REQUIRE(scalarInt(verification,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600205'") == 1);
    REQUIRE(scalarInt(verification,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600206'") == 0);
    REQUIRE(sqlite3_close(verification) == SQLITE_OK);
    REQUIRE(writer.write(replacement, 1, 0, true).rowsWritten == 1);
}

TEST_CASE("sqlite import cancels a blocked commit and rolls back explicitly") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600207"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600207");
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    sqlite3* reader = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &reader) == SQLITE_OK);
    REQUIRE(sqlite3_exec(reader, "PRAGMA journal_mode=DELETE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    REQUIRE(sqlite3_exec(reader, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_stmt* readStatement = nullptr;
    REQUIRE(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM ssa_table", -1, &readStatement,
                               nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(readStatement) == SQLITE_ROW);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600208"}, {"descricao_ssa", "Nova"}});
    replacement.ssaNumbersForUpsertDelete.emplace_back("202600208");
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(writer.write(replacement, 1, 0, true, stopSource.get_token()));
            return std::error_code{};
        } catch (const std::system_error& error) {
            return error.code();
        }
    });

    REQUIRE(operation.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);
    stopSource.request_stop();
    REQUIRE(operation.wait_for(std::chrono::milliseconds{500}) == std::future_status::ready);
    REQUIRE(operation.get() == std::make_error_code(std::errc::operation_canceled));
    REQUIRE(sqlite3_finalize(readStatement) == SQLITE_OK);
    REQUIRE(sqlite3_exec(reader, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(reader) == SQLITE_OK);

    sqlite3* verification = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &verification) == SQLITE_OK);
    REQUIRE(scalarText(verification, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(verification,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600207'") == 1);
    REQUIRE(scalarInt(verification,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600208'") == 0);
    REQUIRE(sqlite3_close(verification) == SQLITE_OK);
    REQUIRE(writer.write(replacement, 1, 0, true).rowsWritten == 1);
}

TEST_CASE("sqlite import survives process death before and after commit") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    for (const bool commitBeforeKill : {false, true}) {
        const auto scenario = commitBeforeKill ? "after" : "before";
        const auto dbPath = root / scenario / "ssas.db";
        const auto readyPath = root / scenario / "ready";
        std::filesystem::create_directories(dbPath.parent_path());

        sqlite3* seed = nullptr;
        REQUIRE(sqlite3_open(dbPath.string().c_str(), &seed) == SQLITE_OK);
        REQUIRE(sqlite3_exec(seed,
                             "CREATE TABLE ssa_table(numero_ssa TEXT, descricao_ssa TEXT, "
                             "id INTEGER PRIMARY KEY);"
                             "INSERT INTO ssa_table(numero_ssa, descricao_ssa) "
                             "VALUES('202600210', 'Anterior');",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_close(seed) == SQLITE_OK);

        QProcess child;
        child.start(QString::fromUtf8(SSA_SQLITE_CRASH_PROBE_PATH),
                    {ssa::qt::toQString(dbPath), ssa::qt::toQString(readyPath), scenario});
        REQUIRE(child.waitForStarted(5000));
        QElapsedTimer deadline;
        deadline.start();
        while (!std::filesystem::exists(readyPath) && deadline.elapsed() < 5000) {
            QThread::msleep(10);
        }
        REQUIRE(std::filesystem::exists(readyPath));
        child.kill();
        REQUIRE(child.waitForFinished(5000));
        REQUIRE(child.exitStatus() == QProcess::CrashExit);

        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
        REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600210'") ==
                (commitBeforeKill ? 0 : 1));
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600211'") ==
                (commitBeforeKill ? 1 : 0));
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_schema WHERE type='index' AND "
                              "name='idx_ssa_table_numero_ssa'") == (commitBeforeKill ? 1 : 0));
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const std::vector<ssa::domain::ColumnDef> columns{
            {.key = "numero_ssa", .label = "Numero", .labelFull = "Numero"},
            {.key = "descricao_ssa", .label = "Descricao", .labelFull = "Descricao"}};
        const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, columns);
        ssa::infra::importing::ResolvedSsaImportRows retry;
        retry.rows.push_back({{"numero_ssa", "202600212"}, {"descricao_ssa", "Retry"}});
        retry.ssaNumbersForUpsertDelete.emplace_back("202600212");
        REQUIRE(writer.write(retry, 1, 0, true).rowsWritten == 1);
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-journal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-wal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-shm"));
    }
}

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

TEST_CASE("incremental rescan consolidates only committed input workbooks") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto noSurvivorDirectory = processedDirectory / "nosurvivor";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600119"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600119");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    const auto validWorkbook = inputDirectory / "valid.xlsx";
    writeWorkbook(validWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600120"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Linha valida")}));
    const auto emptyWorkbook = inputDirectory / "empty.xlsx";
    writeWorkbook(emptyWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}));
    const auto unknown = inputDirectory / "pending.txt";
    {
        std::ofstream output(unknown);
        output << "pending";
    }

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE_FALSE(result.warning);
    REQUIRE(std::filesystem::exists(processedDirectory / "valid.xlsx"));
    REQUIRE(std::filesystem::exists(noSurvivorDirectory / "empty.xlsx"));
    REQUIRE(std::filesystem::exists(unknown));
    REQUIRE_FALSE(std::filesystem::exists(validWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(emptyWorkbook));

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 2);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600119'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("incremental rescan preserves an existing destination with a unique name") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(processedDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto existing = processedDirectory / "same.xlsx";
    {
        std::ofstream output(existing, std::ios::binary);
        output << "existing";
    }
    const auto workbook = inputDirectory / "same.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600121"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Novo arquivo")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(readFile(existing) == "existing");
    REQUIRE(std::filesystem::exists(processedDirectory / "same__1.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(workbook));
}

TEST_CASE("post commit consolidation failure stays visible and preserves the input") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedPath = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    {
        std::ofstream blockedDestination(processedPath);
        blockedDestination << "not a directory";
    }
    const auto workbook = inputDirectory / "committed.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600124"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Commit antes do move")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.message.find("error=consolidation_failed") != std::string::npos);
    REQUIRE(result.diagnostic.find("cannot create consolidation directory") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600124'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite commit failure prevents creation of a consolidation manifest") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600125"}, {"descricao_ssa", "Linha anterior"}});
    seedRows.ssaNumbersForUpsertDelete.emplace_back("202600125");
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);

    sqlite3* readLock = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &readLock) == SQLITE_OK);
    REQUIRE(sqlite3_exec(readLock, "PRAGMA journal_mode=DELETE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    REQUIRE(sqlite3_exec(readLock, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_stmt* lockedStatement = nullptr;
    REQUIRE(sqlite3_prepare_v2(readLock, "SELECT COUNT(*) FROM ssa_table", -1, &lockedStatement,
                               nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(lockedStatement) == SQLITE_ROW);

    const auto workbook = inputDirectory / "blocked-commit.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600126"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Nao consolidar")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(sqlite3_finalize(lockedStatement) == SQLITE_OK);
    REQUIRE(sqlite3_exec(readLock, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(readLock) == SQLITE_OK);
    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* verificationDb = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &verificationDb) == SQLITE_OK);
    REQUIRE(scalarInt(verificationDb,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600126'") == 0);
    REQUIRE(sqlite3_close(verificationDb) == SQLITE_OK);
}

TEST_CASE("external import consolidates its staged copy and preserves the selected source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "selected.xlsx";
    writeWorkbook(source, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600122"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Import externo")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {source};
    const auto result = port.importExternalFiles(request);

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE(directWorkbookCount(inputDirectory) == 0);
    REQUIRE(directWorkbookCount(processedDirectory) == 1);
}

TEST_CASE("spreadsheet import workflow preserves unicode paths") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto asciiWorkbook =
        std::filesystem::path{tempDir.path().toStdString()} / "unicode-source.xlsx";
    writeWorkbook(asciiWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600099"), inlineCell("B2", "APV"),
                              inlineCell("C2", "Caminho unicode")}));

    const QString unicodeRootText =
        tempDir.filePath(QString::fromUtf8("importacao-unicode-\xE6\xBC\xA2"));
    const auto unicodeRoot = ssa::qt::toFileSystemPath(unicodeRootText);
    const auto workbook = ssa::qt::toFileSystemPath(
        QDir(unicodeRootText).filePath(QString::fromUtf8("entrada-\xE6\xBC\xA2.xlsx")));
    const auto inputDirectory = ssa::qt::toFileSystemPath(
        QDir(unicodeRootText).filePath(QString::fromUtf8("documentos-\xE6\xBC\xA2")));
    const auto dbPath = ssa::qt::toFileSystemPath(
        QDir(unicodeRootText).filePath(QString::fromUtf8("dados-\xE6\xBC\xA2/ssas.db")));
    std::filesystem::create_directories(unicodeRoot);
    std::filesystem::create_directories(dbPath.parent_path());
    std::filesystem::rename(asciiWorkbook, workbook);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {workbook};

    const auto result = port.importExternalFiles(request);

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.message.find("rows=1") != std::string::npos);
    ssa::infra::sqlite::SqliteConnection connection(dbPath);
    REQUIRE(scalarInt(connection.handle(), "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarText(connection.handle(), "SELECT numero_ssa FROM ssa_table LIMIT 1") ==
            "202600099");
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
    REQUIRE(result.message.find("error=operation_failed") != std::string::npos);
    REQUIRE(result.diagnostic.find("cannot read xlsx zip package") != std::string::npos);
}

TEST_CASE("spreadsheet import workflow rejects more than 64 files before staging") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    for (std::size_t index = 0; index < 65; ++index) {
        request.files.push_back(root / ("source_" + std::to_string(index) + ".xlsx"));
    }

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("too_many_files max=64") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("import file stager accepts exactly 64 files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    std::filesystem::create_directories(sourceDirectory);
    std::vector<std::filesystem::path> files;
    for (std::size_t index = 0; index < 64; ++index) {
        const auto source = sourceDirectory / ("source_" + std::to_string(index) + ".xlsx");
        createSparseFile(source, 0);
        files.push_back(source);
    }

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const auto result = stager.stageExternalFiles(files);

    REQUIRE(result.rejectionReason.empty());
    REQUIRE(result.failedCopies == 0);
    REQUIRE(result.files.size() == 64);
}

TEST_CASE("spreadsheet import workflow rejects files larger than 128 MiB before staging") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t maxFileBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "oversized.xlsx";
    const auto blockedInputPath = root / "input-is-a-file";
    const auto dbPath = root / "data" / "ssas.db";
    createSparseFile(source, maxFileBytes + 1);
    createSparseFile(blockedInputPath, 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(blockedInputPath, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {source};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("file_too_large max_bytes=134217728") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("spreadsheet import workflow rejects batches larger than 1 GiB before staging") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t maxFileBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto blockedInputPath = root / "input-is-a-file";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    createSparseFile(blockedInputPath, 1);
    ssa::ports::ImportExternalFilesRequest request;
    for (std::size_t index = 0; index < 9; ++index) {
        const auto source = sourceDirectory / ("part_" + std::to_string(index) + ".xlsx");
        createSparseFile(source, maxFileBytes);
        request.files.push_back(source);
    }
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(blockedInputPath, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("batch_too_large max_bytes=1073741824") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
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

    const auto noSurvivorDirectory = inputDirectory / "processadas" / "nosurvivor";
    std::filesystem::create_directories(noSurvivorDirectory);
    writeWorkbook(noSurvivorDirectory / "ignored.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600011"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Nao deve voltar")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    REQUIRE(port.rescan({ssa::ports::RescanMode::Full}).ok());
    REQUIRE(std::filesystem::exists(inputDirectory / "processadas" / "first.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(workbook));
    REQUIRE(port.rescan({ssa::ports::RescanMode::Full}).ok());

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600011'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("full rescan preserves database when legacy conversion fails") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600399"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600399");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    {
        std::ofstream legacy(inputDirectory / "broken.xls");
        legacy << "not a workbook";
    }
    writeWorkbook(inputDirectory / "valid.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600400"), inlineCell("B2", "Nova")}));
    ssa::infra::importing::LegacySpreadsheetConverter converter(root / "missing-soffice");
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns(), converter);

    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600399'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600400'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

#ifndef _WIN32
TEST_CASE("full rescan rejects a symlinked processed directory without clearing the database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto outsideDirectory = root / "outside";
    const auto seedDirectory = root / "seed";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(outsideDirectory);
    std::filesystem::create_directories(seedDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto seedWorkbook = seedDirectory / "seed.xlsx";
    writeWorkbook(seedWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600126"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Linha preservada")}));
    writeWorkbook(outsideDirectory / "outside.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600127"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Fora da entrada")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest seedRequest;
    seedRequest.files = {seedWorkbook};
    REQUIRE(port.importExternalFiles(seedRequest).ok());
    REQUIRE(std::filesystem::remove_all(inputDirectory / "processadas") > 0);
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    std::filesystem::create_directory_symlink(outsideDirectory, inputDirectory / "processadas");

    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("processed_directory_symlink") != std::string::npos);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600126'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    REQUIRE(std::filesystem::exists(outsideDirectory / "outside.xlsx"));
}
#endif

TEST_CASE("full rescan rejects empty input without clearing the database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600401"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600401");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("no_importable_files") != std::string::npos);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600401'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("full rescan rejects an unrecognized header without clearing or moving the source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600402"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600402");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    const auto workbook = inputDirectory / "unknown-header.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Unknown A"), inlineCell("B1", "Unknown B"),
                                    inlineCell("C1", "Unknown C")}) +
                                row(2, {inlineCell("A2", "one"), inlineCell("B2", "two"),
                                        inlineCell("C2", "three")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("header_not_recognized") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600402'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("full rescan rolls back a valid workbook when a later header is unrecognized") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600404"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600404");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    const auto validWorkbook = inputDirectory / "a-valid.xlsx";
    writeWorkbook(validWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600405"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Nova")}));
    const auto invalidWorkbook = inputDirectory / "b-invalid.xlsx";
    writeWorkbook(invalidWorkbook,
                  row(1, {inlineCell("A1", "Unknown A"), inlineCell("B1", "Unknown B"),
                          inlineCell("C1", "Unknown C")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("header_not_recognized") != std::string::npos);
    REQUIRE(std::filesystem::exists(validWorkbook));
    REQUIRE(std::filesystem::exists(invalidWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600404'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600405'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("full rescan rejects workbooks without valid rows and preserves their sources") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600403"}, {"descricao_ssa", "Anterior"}});
    previous.ssaNumbersForUpsertDelete.emplace_back("202600403");
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    const auto workbook = inputDirectory / "header-only.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("no_valid_rows") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600403'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("full rescan reports database open failure for a valid workbook") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "missing-parent" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    const auto workbook = inputDirectory / "valid.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600406"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Nova")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("operation_failed") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(std::filesystem::exists(workbook));
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

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("legacy_xls=1") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
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
    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files.front().workbookPath == workbook);
    REQUIRE(result.files.front().consolidationSources ==
            std::vector<std::filesystem::path>{workbook});
}

TEST_CASE("incremental rescan keeps an unrelated legacy file pending beside an existing xlsx") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto legacy = inputDirectory / "legacy.xls";
    const auto workbook = inputDirectory / "legacy.xlsx";
    {
        std::ofstream output(legacy);
        output << "independent legacy content";
    }
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA")}) +
                                row(2, {inlineCell("A2", "202600128"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Existing workbook")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::exists(legacy));
    REQUIRE_FALSE(std::filesystem::exists(workbook));
    REQUIRE(std::filesystem::exists(processedDirectory / workbook.filename()));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600128'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
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
    REQUIRE(result.files.empty());
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

TEST_CASE("incremental rescan consolidates the legacy source and converted workbook") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto legacyWorkbook = inputDirectory / "legacy-input.xls";
    writeWorkbook(legacyWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600123"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Legado da entrada")}));

    const auto fakeSoffice = writeFakeSoffice(root);
    ssa::infra::importing::LegacySpreadsheetConverter converter(fakeSoffice);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns(), converter);
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::exists(processedDirectory / "legacy-input.xls"));
    REQUIRE(std::filesystem::exists(processedDirectory / "legacy-input.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(legacyWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "legacy-input.xlsx"));
}
#endif
