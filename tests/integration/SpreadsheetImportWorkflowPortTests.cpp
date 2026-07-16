#include "application/SsaWorkflowService.h"
#include "domain/ColumnCatalog.h"
#include "infra/import/CancelableFileCopy.h"
#include "infra/import/ImportFileStager.h"
#include "infra/import/LegacySpreadsheetConverter.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/import/SsaImportConflictResolver.h"
#include "infra/import/SsaSpreadsheetMapper.h"
#include "infra/import/XlsxPackage.h"
#include "infra/import/XlsxWorkbookReader.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseValidator.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "platform/SupervisedProcessRunner.h"
#include "qt/FilesystemPath.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <miniz.h>
#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

    class StaticSamPort final : public ssa::ports::ISamRefreshPort {
      public:
        ssa::ports::SamFetchResult fetch(const ssa::ports::SamRefreshRequest&,
                                         std::stop_token = {}) override {
            return result;
        }

        bool discardArtifacts() override {
            ++discardCalls;
            return true;
        }

        ssa::ports::SamFetchResult result;
        int discardCalls = 0;
    };

    std::vector<ssa::domain::ColumnDef> importColumns() {
        const auto columns = ssa::domain::ColumnCatalog::all();
        return {columns.begin(), columns.end()};
    }

    std::filesystem::path databaseImportLockPathForTest(const std::filesystem::path& databasePath) {
        std::error_code error;
        auto normalized = std::filesystem::weakly_canonical(databasePath, error);
        if (error) {
            normalized = std::filesystem::absolute(databasePath).lexically_normal();
        }
        const auto digest =
            QCryptographicHash::hash(QString::fromStdString(normalized.string()).toUtf8(),
                                     QCryptographicHash::Sha256)
                .toHex()
                .toStdString();
        return std::filesystem::path{QDir::tempPath().toStdString()} /
               (".ssa_import_db_" + digest + ".lock");
    }

    void addZipEntry(mz_zip_archive& zip, const char* path, const std::string& content) {
        REQUIRE(mz_zip_writer_add_mem(&zip, path, content.data(), content.size(),
                                      MZ_BEST_COMPRESSION) != 0);
    }

    void createSparseFile(const std::filesystem::path& path, std::uintmax_t size);

    void writeWorkbook(const std::filesystem::path& path, const std::string& rowsXml,
                       const std::uintmax_t paddingBytes = 0,
                       const std::string& sharedStringsXml = {}) {
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
        if (!sharedStringsXml.empty()) {
            addZipEntry(zip, "xl/sharedStrings.xml", sharedStringsXml);
        }
        addZipEntry(zip, "xl/worksheets/sheet1.xml",
                    R"(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<dimension ref="A1:E4"/><sheetData>)" +
                        rowsXml + "</sheetData></worksheet>");
        const auto paddingPath = path.string() + ".padding";
        if (paddingBytes > 0) {
            createSparseFile(paddingPath, paddingBytes);
            REQUIRE(mz_zip_writer_add_file(&zip, "padding.bin", paddingPath.c_str(), nullptr, 0,
                                           MZ_NO_COMPRESSION) != 0);
        }
        REQUIRE(mz_zip_writer_finalize_archive(&zip) != 0);
        REQUIRE(mz_zip_writer_end(&zip) != 0);
        if (paddingBytes > 0) {
            REQUIRE(std::filesystem::remove(paddingPath));
        }
    }

    void writeWorkbookSheets(const std::filesystem::path& path,
                             const std::vector<std::string>& sheets) {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0);
        addZipEntry(zip, "[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
</Types>)");
        std::string workbook = R"(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>)";
        std::string relationships = R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
        for (std::size_t index = 0; index < sheets.size(); ++index) {
            const auto number = std::to_string(index + 1);
            workbook += "<sheet name=\"Sheet" + number + "\" sheetId=\"" + number +
                        "\" r:id=\"rId" + number + "\"/>";
            relationships += "<Relationship Id=\"rId" + number +
                             "\" "
                             "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
                             "relationships/worksheet\" Target=\"worksheets/sheet" +
                             number + ".xml\"/>";
            addZipEntry(zip, ("xl/worksheets/sheet" + number + ".xml").c_str(),
                        R"(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)" +
                            sheets[index] + "</sheetData></worksheet>");
        }
        workbook += "</sheets></workbook>";
        relationships += "</Relationships>";
        addZipEntry(zip, "xl/workbook.xml", workbook);
        addZipEntry(zip, "xl/_rels/workbook.xml.rels", relationships);
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

    std::string samWorkbookRows(const std::string& number, const std::string& executor,
                                const std::string& description) {
        return row(1, {inlineCell("A1", "ssa_number"), inlineCell("B1", "localization"),
                       inlineCell("C1", "description"), inlineCell("D1", "issue_datetime"),
                       inlineCell("E1", "emission_datetime"), inlineCell("F1", "emitter_sector"),
                       inlineCell("G1", "executor_sector"), inlineCell("H1", "year_week"),
                       inlineCell("I1", "situation_desc"), inlineCell("J1", "process_status"),
                       inlineCell("K1", "detail_present")}) +
               row(2, {inlineCell("A2", number), inlineCell("B2", "LOC-SAM"),
                       inlineCell("C2", description), inlineCell("D2", "2026-07-15T08:00:00Z"),
                       inlineCell("E2", "2026-07-15T09:00:00Z"), inlineCell("F2", "IEE2"),
                       inlineCell("G2", executor), inlineCell("H2", "202629"),
                       inlineCell("I2", "STE - SERVICO TERMINADO"),
                       inlineCell("J2", "SSA Terminada"), inlineCell("K2", "true")});
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

    void setLocalModificationTime(const std::filesystem::path& path, const int year,
                                  const int month, const int day, const int hour,
                                  const int minute) {
        std::tm local{};
        local.tm_year = year - 1900;
        local.tm_mon = month - 1;
        local.tm_mday = day;
        local.tm_hour = hour;
        local.tm_min = minute;
        local.tm_isdst = -1;
        const auto timestamp = std::mktime(&local);
        REQUIRE(timestamp != static_cast<std::time_t>(-1));
        std::filesystem::last_write_time(path,
                                         std::filesystem::file_time_type::clock::from_sys(
                                             std::chrono::system_clock::from_time_t(timestamp)));
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
                      "printf 'ready' > \"$source.conversion-ready\"\n"
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
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {root / "source.xlsx"};
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = port.importExternalFiles(request, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 1);
    REQUIRE(result.importSummary->preserved == 1);
    REQUIRE(result.importSummary->files.size() == 1);
    REQUIRE(result.importSummary->files.front().source == "source.xlsx");
    REQUIRE(result.importSummary->files.front().status == ssa::ports::ImportFileStatus::Canceled);
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("external import preflight preserves the rejected XLSX source in its summary") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto missingWorkbook = root / "missing.xlsx";
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(
        inputDirectory, root / "data" / "ssas.db", importColumns());

    const auto result = port.importExternalFiles({{missingWorkbook}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 1);
    REQUIRE(result.importSummary->rejected == 1);
    REQUIRE(result.importSummary->preserved == 1);
    REQUIRE(result.importSummary->files.size() == 1);
    REQUIRE(result.importSummary->files.front().source == "missing.xlsx");
    REQUIRE(result.importSummary->files.front().status == ssa::ports::ImportFileStatus::Failed);
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
}

TEST_CASE("external import rejects a symlink source before copying") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "source.xlsx";
    const auto link = root / "selected.xlsx";
    createSparseFile(source, 1);
    std::error_code error;
    std::filesystem::create_symlink(source, link, error);
    if (error) {
        SKIP("symbolic links are unavailable in this environment");
    }

    const ssa::infra::importing::ImportFileStager stager(root / "docs_entrada");
    const auto result = stager.stageExternalFiles({link});

    REQUIRE(result.rejectionReason == "source_symlink");
    REQUIRE(result.files.empty());
    REQUIRE_FALSE(std::filesystem::exists(root / "docs_entrada"));
    REQUIRE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(link));
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

TEST_CASE("input preflight preserves XLSX inventory and pending XLS classification") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    const auto oversized = inputDirectory / "oversized.xlsx";
    const auto legacy = inputDirectory / "pending.xls";
    createSparseFile(oversized, 128ULL * 1024ULL * 1024ULL + 1ULL);
    createSparseFile(legacy, 1);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);

    const auto result = stager.stageInputFiles();

    REQUIRE(result.rejectionReason == "file_too_large max_bytes=134217728");
    REQUIRE(result.discovered == 2);
    REQUIRE(result.discoveredXlsxSources == std::vector<std::string>{"oversized.xlsx"});
    REQUIRE(result.legacyXls == 1);
    REQUIRE(result.unsupported == 0);
    REQUIRE(result.files.empty());
    REQUIRE(std::filesystem::exists(oversized));
    REQUIRE(std::filesystem::exists(legacy));
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

TEST_CASE("file copy rejects a source changed during the staged copy") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "changing-source.xlsx";
    const auto destination = root / "docs_entrada" / "staged.xlsx";
    createSparseFile(source, copyBytes);

    auto future = std::async(std::launch::async, [&] {
        return ssa::infra::importing::copyFileAtomically({source, destination});
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporary = false;
    while (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(destination.parent_path(), error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") != std::string::npos) {
                observedTemporary = true;
                std::filesystem::last_write_time(
                    source, std::filesystem::file_time_type::clock::now(), error);
                REQUIRE_FALSE(error);
                break;
            }
        }
        QThread::msleep(1);
    }
    const auto result = future.get();

    REQUIRE(observedTemporary);
    REQUIRE(result.status == ssa::infra::importing::FileCopyStatus::Failed);
    REQUIRE(result.diagnostic == "source changed during staged file copy");
    REQUIRE_FALSE(std::filesystem::exists(destination));
}

TEST_CASE("file copy rejects a source replaced with the same size and mtime") {
#ifdef _WIN32
    SKIP("replacing an open source is not supported by this Windows fixture");
#else
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "identity-source.xlsx";
    const auto replacement = root / "identity-replacement.xlsx";
    const auto backup = root / "identity-original.xlsx";
    const auto destination = root / "docs_entrada" / "staged.xlsx";
    createSparseFile(source, copyBytes);
    std::error_code error;
    const auto originalTime = std::filesystem::last_write_time(source, error);
    REQUIRE_FALSE(error);

    auto future = std::async(std::launch::async, [&] {
        return ssa::infra::importing::copyFileAtomically({source, destination});
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporary = false;
    bool replaced = false;
    while (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        for (std::filesystem::directory_iterator iterator(destination.parent_path(), error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") == std::string::npos) {
                continue;
            }
            observedTemporary = true;
            createSparseFile(replacement, copyBytes);
            std::filesystem::last_write_time(replacement, originalTime, error);
            REQUIRE_FALSE(error);
            std::filesystem::rename(source, backup, error);
            REQUIRE_FALSE(error);
            std::filesystem::rename(replacement, source, error);
            REQUIRE_FALSE(error);
            replaced = true;
            break;
        }
        error.clear();
        QThread::msleep(1);
    }
    const auto result = future.get();

    REQUIRE(observedTemporary);
    REQUIRE(replaced);
    REQUIRE(result.status == ssa::infra::importing::FileCopyStatus::Failed);
    REQUIRE(result.diagnostic == "source changed during staged file copy");
    REQUIRE_FALSE(std::filesystem::exists(destination));
#endif
}

TEST_CASE("import file stager preserves inventory after a later source disappears") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto firstSource = root / "first.xlsx";
    const auto missingSource = root / "second.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    createSparseFile(firstSource, copyBytes);
    createSparseFile(missingSource, 1);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);

    auto future = std::async(std::launch::async, [&] {
        return stager.stageExternalFiles({firstSource, missingSource});
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
                REQUIRE(std::filesystem::remove(missingSource));
                break;
            }
        }
        if (observedTemporary) {
            break;
        }
        QThread::msleep(1);
    }
    const auto result = future.get();

    REQUIRE(observedTemporary);
    REQUIRE(result.discovered == 2);
    REQUIRE(result.discoveredXlsxSources == std::vector<std::string>{"first.xlsx", "second.xlsx"});
    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files.front().summaryIndex == 0);
    REQUIRE(result.failedCopies == 1);
}

TEST_CASE("external import reports one applied and one failed staging source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t paddingBytes = 96ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto firstSource = root / "first.xlsx";
    const auto missingSource = root / "second.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(dbPath.parent_path());
    const auto header =
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao da SSA"),
                inlineCell("C1", "Data de emissao")});
    writeWorkbook(firstSource,
                  header +
                      row(2, {inlineCell("A2", "202600888"), inlineCell("B2", "Fonte aplicada"),
                              inlineCell("C2", "2026-07-14")}),
                  paddingBytes);
    writeWorkbook(missingSource, header + row(2, {inlineCell("A2", "202600889"),
                                                  inlineCell("B2", "Fonte removida"),
                                                  inlineCell("C2", "2026-07-14")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {firstSource, missingSource};

    auto future = std::async(std::launch::async, [&] { return port.importExternalFiles(request); });
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
                REQUIRE(std::filesystem::remove(missingSource));
                break;
            }
        }
        if (observedTemporary) {
            break;
        }
        QThread::msleep(1);
    }
    const auto result = future.get();

    INFO(result.message);
    INFO(result.diagnostic);
    REQUIRE(observedTemporary);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.importSummary.has_value());
    const auto& summary = *result.importSummary;
    REQUIRE(summary.discovered == 2);
    REQUIRE(summary.accepted == 1);
    REQUIRE(summary.rejected == 1);
    REQUIRE(summary.preserved == 1);
    REQUIRE(summary.files.size() == 2);
    REQUIRE(summary.files[0].status == ssa::ports::ImportFileStatus::Applied);
    REQUIRE(summary.files[1].status == ssa::ports::ImportFileStatus::Failed);
}

TEST_CASE("staging cancellation stays clean with a pending legacy workbook") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto legacy = root / "broken.xls";
    const auto largeSource = root / "large-source.xlsx";
    const auto inputDirectory = root / "docs_entrada";
    createSparseFile(legacy, 1);
    createSparseFile(largeSource, copyBytes);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
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
    REQUIRE(result.diagnostic.empty());
    REQUIRE(result.files.empty());
    REQUIRE(std::filesystem::exists(legacy));
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
TEST_CASE("workflow preserves canceled status when owned staging cleanup cannot complete") {
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
    constexpr std::size_t bufferedEntryLimit = 32ULL * 1024ULL * 1024ULL;
    writeWorkbook(source, std::string(bufferedEntryLimit + 1, ' ') +
                              row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                      inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600410"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Cleanup bloqueado")}));
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write({}, 0, 0, false).rowsWritten == 0);
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

    CAPTURE(result.message, result.diagnostic);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE(result.diagnostic.find("operation=remove_owned_staging") != std::string::npos);
    REQUIRE(result.diagnostic.find("path=" + inputDirectory.string()) != std::string::npos);
    REQUIRE(result.diagnostic.find("error=") != std::string::npos);
    REQUIRE(result.diagnostic.find("pending=true") != std::string::npos);
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
    REQUIRE(result.diagnostic.find("operation=remove_copy_temporary") != std::string::npos);
    REQUIRE(result.diagnostic.find("path=" + inputDirectory.string()) != std::string::npos);
    REQUIRE(result.diagnostic.find("error=") != std::string::npos);
    REQUIRE(result.diagnostic.find("pending=true") != std::string::npos);
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

TEST_CASE("consolidation preflight counts failed sources rather than manifest entries") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    {
        std::ofstream blocked(inputDirectory / "processadas");
        blocked << "not a directory";
    }
    const auto legacy = inputDirectory / "one.xls";
    const auto converted = inputDirectory / "one.xlsx";
    const auto workbook = inputDirectory / "two.xlsx";
    createSparseFile(legacy, 1);
    createSparseFile(converted, 1);
    createSparseFile(workbook, 1);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const std::vector<ssa::infra::importing::ImportManifestEntry> manifest{
        {{legacy, converted, workbook}, true}, {{}, false}};

    const auto result = stager.consolidate(manifest);

    REQUIRE(result.failed == 3);
    REQUIRE(result.moved == 0);
    REQUIRE(result.error.find("cannot create consolidation directory") != std::string::npos);
}

TEST_CASE("partial consolidation reports each manifest entry independently") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    const auto first = inputDirectory / "first.xlsx";
    const auto missing = inputDirectory / "missing.xlsx";
    createSparseFile(first, 1);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const std::vector<ssa::infra::importing::ImportManifestEntry> manifest{{{first}, true},
                                                                           {{missing}, true}};

    const auto result = stager.consolidate(manifest);

    REQUIRE(result.moved == 1);
    REQUIRE(result.failed == 1);
    REQUIRE(result.entries.size() == 2);
    REQUIRE(result.entries[0].moved == 1);
    REQUIRE(result.entries[0].failed == 0);
    REQUIRE(result.entries[0].noSurvivor == 0);
    REQUIRE(result.entries[1].moved == 0);
    REQUIRE(result.entries[1].failed == 1);
    REQUIRE(result.entries[1].noSurvivor == 0);
    REQUIRE(std::filesystem::exists(inputDirectory / "processadas" / "first.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(first));
}

TEST_CASE("consolidation journal rejects paths outside the input root") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto outsideSource = root / "outside.xlsx";
    const auto outsideDestination = root / "moved.xlsx";
    std::filesystem::create_directories(inputDirectory);
    createSparseFile(outsideSource, 1);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    ssa::infra::importing::ImportConsolidationPlan plan;
    plan.entries.push_back({{{outsideSource, outsideDestination, true}}});

    const auto result = stager.consolidate(plan);

    REQUIRE(result.failed == 1);
    REQUIRE(result.error.find("outside the input root") != std::string::npos);
    REQUIRE(std::filesystem::exists(outsideSource));
    REQUIRE_FALSE(std::filesystem::exists(outsideDestination));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
}

TEST_CASE("legacy converter rejects a stopped token before starting a process") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const ssa::infra::importing::LegacySpreadsheetConverter converter(root / "soffice", nullptr);
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = converter.convertToXlsx({root / "source.xls", root / "output.xlsx"},
                                                stopSource.get_token());

    REQUIRE(result.status == ssa::infra::importing::LegacySpreadsheetConversionStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(root / "output.xlsx"));
}

#ifndef _WIN32
TEST_CASE("legacy converter treats a configured directory as unavailable") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const ssa::infra::importing::LegacySpreadsheetConverter converter(root, nullptr);

    const auto result = converter.convertToXlsx({root / "source.xls", root / "output.xlsx"});

    REQUIRE(result.status ==
            ssa::infra::importing::LegacySpreadsheetConversionStatus::ToolUnavailable);
    REQUIRE(result.message == "xls converter unavailable");
}
#endif

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
    const auto processRunner = std::make_shared<ssa::platform::SupervisedProcessRunner>();
    const ssa::infra::importing::LegacySpreadsheetConverter converter(
        writeFakeSoffice(root, FakeSofficeBehavior::Block), processRunner);
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return converter.convertToXlsx({source, destination}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    const auto conversionReady = source.string() + ".conversion-ready";
    while (operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           !std::filesystem::exists(conversionReady) && deadline.elapsed() < 10'000) {
        QThread::msleep(5);
    }
    if (std::filesystem::exists(conversionReady)) {
        stopSource.request_stop();
    }
    stopSource.request_stop();
    const auto result = operation.get();

    CAPTURE(static_cast<int>(result.status), result.message, result.diagnostic);
    REQUIRE(std::filesystem::exists(conversionReady));
    REQUIRE(result.status == ssa::infra::importing::LegacySpreadsheetConversionStatus::Canceled);
    REQUIRE(readFile(destination) == "previous");
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        REQUIRE_FALSE(entry.path().filename().string().starts_with("ssa_xls_conversion_"));
    }
}

TEST_CASE("legacy converter reports cleanup failure after canceled output copy") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t copyBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto source = root / "source.xls";
    const auto destination = root / "output.xlsx";
    createSparseFile(source, copyBytes);
    const auto processRunner = std::make_shared<ssa::platform::SupervisedProcessRunner>();
    const ssa::infra::importing::LegacySpreadsheetConverter converter(
        writeFakeSoffice(root, FakeSofficeBehavior::CleanupFailure), processRunner);
    std::stop_source stopSource;
    auto operation = std::async(std::launch::async, [&] {
        return converter.convertToXlsx({source, destination}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedCopy = false;
    while (operation.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 3'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(root, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename().string().find(".part") != std::string::npos &&
                iterator->file_size(error) > 0 && !error) {
                observedCopy = true;
                stopSource.request_stop();
                break;
            }
        }
        QThread::msleep(1);
    }
    stopSource.request_stop();
    const auto result = operation.get();

    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory() &&
            entry.path().filename().string().starts_with("ssa_xls_conversion_")) {
            std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
            REQUIRE(std::filesystem::remove_all(entry.path()) > 0);
        }
    }
    REQUIRE(observedCopy);
    REQUIRE(result.status ==
            ssa::infra::importing::LegacySpreadsheetConversionStatus::CleanupFailed);
    REQUIRE(result.diagnostic.find("cannot remove xls conversion temporary directory") !=
            std::string::npos);
    REQUIRE(result.diagnostic.find("path=") != std::string::npos);
    REQUIRE(result.diagnostic.find("error=") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(destination));
}
#endif

TEST_CASE("input staging leaves legacy workbooks pending") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    const auto firstLegacy = inputDirectory / "a.xls";
    const auto secondLegacy = inputDirectory / "b.xls";
    const auto workbook = inputDirectory / "keep.xlsx";
    writeWorkbook(firstLegacy, row(1, {inlineCell("A1", "Numero SSA")}));
    writeWorkbook(secondLegacy, row(1, {inlineCell("A1", "Numero SSA")}));
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA")}));

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const auto result = stager.stageInputFiles();

    REQUIRE(result.rejectionReason.empty());
    REQUIRE_FALSE(result.operationalFailure);
    REQUIRE(result.legacyXls == 2);
    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files.front().workbookPath == workbook);
    REQUIRE(std::filesystem::exists(firstLegacy));
    REQUIRE(std::filesystem::exists(secondLegacy));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "a.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "b.xlsx"));
}

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

TEST_CASE("xlsx reader enforces the shared string count limit") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto workbook = std::filesystem::path{tempDir.path().toStdString()} / "shared-limit.xlsx";
    writeWorkbook(workbook, row(1, {"<c r=\"A1\" t=\"s\"><v>0</v></c>"}), 0,
                  R"(<?xml version="1.0" encoding="UTF-8"?>
<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" uniqueCount="1000001">
</sst>)");

    REQUIRE_THROWS_WITH(ssa::infra::importing::XlsxWorkbookReader::readFirstSheet(workbook),
                        "xlsx shared strings exceed supported limit");
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

TEST_CASE("spreadsheet workflow imports a worksheet larger than the buffered entry limit") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    constexpr std::size_t bufferedEntryLimit = 32ULL * 1024ULL * 1024ULL;
    const auto workbook = inputDirectory / "large-entry.xlsx";
    const auto rowsXml = std::string(bufferedEntryLimit + 1, ' ') +
                         row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                                 inlineCell("C1", "Data Cadastro")}) +
                         row(2, {inlineCell("A2", "202600301"), inlineCell("B2", "Streamed"),
                                 inlineCell("C2", "2026-07-14")});
    writeWorkbook(workbook, rowsXml);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600301'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet workflow counts equal duplicates across chunk boundaries") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    std::string rowsXml = row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                                  inlineCell("C1", "Data Cadastro")});
    for (std::size_t index = 0; index < 1'001; ++index) {
        const auto rowNumber = index + 2;
        const auto suffix = std::to_string(rowNumber);
        const auto numberRef = "A" + suffix;
        const auto descriptionRef = "B" + suffix;
        const auto dateRef = "C" + suffix;
        rowsXml += row(rowNumber, {inlineCell(numberRef.c_str(), "202600302"),
                                   inlineCell(descriptionRef.c_str(), "Duplicate"),
                                   inlineCell(dateRef.c_str(), "2026-07-14")});
    }
    writeWorkbook(inputDirectory / "duplicates.xlsx", rowsXml);
    writeWorkbook(inputDirectory / "z-empty.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                          inlineCell("C1", "Data Cadastro")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.message.find("duplicates=1000") != std::string::npos);
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
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600201"}, {"descricao_ssa", "Nova"}});
    std::stop_source stopSource;
    {
        auto session = writer.startSession(true, stopSource.get_token());
        REQUIRE(session.write(replacement, 1, 0).conflictRows == 0);
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
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600206"}, {"descricao_ssa", "Nova"}});
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
        REQUIRE(writer.write(retry, 1, 0, true).rowsWritten == 1);
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-journal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-wal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-shm"));
    }
}

TEST_CASE("sqlite consolidation journal rolls back and commits with the import session") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "ssas.db";
    const auto source = root / "docs_entrada" / "pending.xlsx";
    const auto destination = root / "docs_entrada" / "processadas" / "pending.xlsx";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows rows;
    rows.rows.push_back({{"numero_ssa", "202600213"}, {"descricao_ssa", "Journal"}});

    {
        auto session = writer.startSession(false);
        REQUIRE(session.write(rows, 1, 0).rowsWritten == 1);
        session.recordConsolidation({{{source}, {destination}, true}});
        session.rollback();
    }
    REQUIRE(writer.pendingConsolidation().empty());

    {
        auto session = writer.startSession(false);
        REQUIRE(session.write(rows, 1, 0).rowsWritten == 1);
        session.recordConsolidation({{{source}, {destination}, true}});
        static_cast<void>(session.finish());
    }
    const auto pending = writer.pendingConsolidation();
    REQUIRE(pending.size() == 1);
    REQUIRE(pending.front().source == std::filesystem::absolute(source).lexically_normal());
    REQUIRE(pending.front().destination ==
            std::filesystem::absolute(destination).lexically_normal());
    REQUIRE(pending.front().hasValidRows);
}

TEST_CASE("pending consolidation rejects an already stopped token on an unlocked database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "ssas.db";
    const auto source = root / "docs_entrada" / "pending.xlsx";
    const auto destination = root / "docs_entrada" / "processadas" / "pending.xlsx";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation({{{source, destination, true}}});
    static_cast<void>(session.finish());
    std::stop_source stopSource;
    stopSource.request_stop();

    try {
        static_cast<void>(writer.pendingConsolidation(stopSource.get_token()));
        FAIL("pending consolidation accepted an already stopped token");
    } catch (const std::system_error& error) {
        REQUIRE(error.code() == std::make_error_code(std::errc::operation_canceled));
    }
}

TEST_CASE("consolidation journal cleanup is one bounded atomic batch") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto dbPath = root / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    std::vector<ssa::infra::importing::ImportConsolidationMove> moves;
    std::vector<std::filesystem::path> sources;
    for (std::size_t index = 0; index < 65; ++index) {
        const auto source = root / "docs_entrada" / ("source_" + std::to_string(index) + ".xlsx");
        const auto destination = root / "docs_entrada" / "processadas" / source.filename();
        moves.push_back({source, destination, true});
        sources.push_back(source);
    }
    auto session = writer.startSession(false);
    session.recordConsolidation(moves);
    static_cast<void>(session.finish());
    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    QElapsedTimer elapsed;
    elapsed.start();

    REQUIRE_THROWS(writer.completeConsolidation(sources));

    REQUIRE(elapsed.elapsed() < 1'000);
    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(writer.pendingConsolidation().size() == 65);
    writer.completeConsolidation(sources);
    REQUIRE(writer.pendingConsolidation().empty());
}

TEST_CASE("spreadsheet workflow resumes committed consolidation after process death") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    for (const auto scenario : {"journal-before-move", "journal-after-move"}) {
        const auto scenarioRoot = root / scenario;
        const auto inputDirectory = scenarioRoot / "docs_entrada";
        const auto dbPath = scenarioRoot / "data" / "ssas.db";
        const auto readyPath = scenarioRoot / "data" / "ready";
        const auto source = inputDirectory / "pending.xlsx";
        const auto destination = inputDirectory / "processadas" / "pending.xlsx";
        std::filesystem::create_directories(inputDirectory);
        std::filesystem::create_directories(dbPath.parent_path());
        createSparseFile(source, 1);

        QProcess child;
        child.start(QString::fromUtf8(SSA_SQLITE_CRASH_PROBE_PATH),
                    {ssa::qt::toQString(dbPath), ssa::qt::toQString(readyPath), scenario,
                     ssa::qt::toQString(source), ssa::qt::toQString(destination)});
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
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_import_consolidation_journal") == 1);
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                                  importColumns());
        const auto resumed = port.rescan({ssa::ports::RescanMode::Incremental});

        INFO(resumed.message);
        INFO(resumed.diagnostic);
        REQUIRE(resumed.status == ssa::ports::WorkflowStatus::Succeeded);
        REQUIRE_FALSE(std::filesystem::exists(source));
        REQUIRE(std::filesystem::exists(destination));
        REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_import_consolidation_journal") == 0);
        REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        const auto second = port.rescan({ssa::ports::RescanMode::Incremental});
        REQUIRE(second.status == ssa::ports::WorkflowStatus::Rejected);
        REQUIRE(std::filesystem::exists(destination));
    }
}

TEST_CASE("rescan resumes completed journal moves and keeps partial failure resumable") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto firstSource = inputDirectory / "first.xlsx";
    const auto secondSource = inputDirectory / "second.xlsx";
    const auto firstDestination = inputDirectory / "processadas" / "first.xlsx";
    const auto secondDestination = inputDirectory / "processadas" / "second.xlsx";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    createSparseFile(firstSource, 1);

    ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation(
        {{{firstSource, firstDestination, true}, {secondSource, secondDestination, true}}});
    static_cast<void>(session.finish());

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto partial = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(partial.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(partial.warning);
    REQUIRE(std::filesystem::exists(firstDestination));
    REQUIRE_FALSE(std::filesystem::exists(firstSource));
    REQUIRE_FALSE(std::filesystem::exists(secondDestination));
    REQUIRE(writer.pendingConsolidation().size() == 1);

    createSparseFile(secondSource, 1);
    const auto resumed = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(resumed.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE_FALSE(resumed.warning);
    REQUIRE(std::filesystem::exists(secondDestination));
    REQUIRE_FALSE(std::filesystem::exists(secondSource));
    REQUIRE(writer.pendingConsolidation().empty());
}

TEST_CASE("rescan resumes a journal before importing the current input batch") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto pendingSource = inputDirectory / "pending.xlsx";
    const auto pendingDestination = inputDirectory / "processadas" / "pending.xlsx";
    const auto currentSource = inputDirectory / "current.xlsx";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    createSparseFile(pendingSource, 1);

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation({{{pendingSource, pendingDestination, true}}});
    static_cast<void>(session.finish());

    writeWorkbook(
        currentSource,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600911"), inlineCell("B2", "APV"),
                    inlineCell("C2", "Current input batch"), inlineCell("D2", "2026-07-16")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    INFO(result.diagnostic);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE_FALSE(result.warning);
    REQUIRE(std::filesystem::exists(pendingDestination));
    REQUIRE_FALSE(std::filesystem::exists(pendingSource));
    REQUIRE_FALSE(std::filesystem::exists(currentSource));
    REQUIRE(writer.pendingConsolidation().empty());

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600911'") == 1);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite consolidation journal survives process death around cleanup commit") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    for (const auto scenario : {"journal-delete-before-commit", "journal-delete-after-commit"}) {
        const bool committed = std::string_view{scenario} == "journal-delete-after-commit";
        const auto scenarioRoot = root / scenario;
        const auto dbPath = scenarioRoot / "ssas.db";
        const auto readyPath = scenarioRoot / "ready";
        const auto source = scenarioRoot / "docs_entrada" / "pending.xlsx";
        const auto destination = scenarioRoot / "docs_entrada" / "processadas" / "pending.xlsx";
        std::filesystem::create_directories(source.parent_path());
        createSparseFile(source, 1);
        const std::vector<ssa::domain::ColumnDef> columns{
            {.key = "numero_ssa", .label = "Numero", .labelFull = "Numero"},
            {.key = "descricao_ssa", .label = "Descricao", .labelFull = "Descricao"}};
        const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, columns);
        auto session = writer.startSession(false);
        session.recordConsolidation({{{source, destination, true}}});
        static_cast<void>(session.finish());

        sqlite3* reader = nullptr;
        sqlite3_stmt* readStatement = nullptr;
        if (!committed) {
            REQUIRE(sqlite3_open(dbPath.string().c_str(), &reader) == SQLITE_OK);
            REQUIRE(sqlite3_exec(reader, "PRAGMA journal_mode=DELETE", nullptr, nullptr, nullptr) ==
                    SQLITE_OK);
            REQUIRE(sqlite3_exec(reader, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK);
            REQUIRE(sqlite3_prepare_v2(reader,
                                       "SELECT COUNT(*) FROM ssa_import_consolidation_journal", -1,
                                       &readStatement, nullptr) == SQLITE_OK);
            REQUIRE(sqlite3_step(readStatement) == SQLITE_ROW);
        }

        QProcess child;
        child.start(QString::fromUtf8(SSA_SQLITE_CRASH_PROBE_PATH),
                    {ssa::qt::toQString(dbPath), ssa::qt::toQString(readyPath), scenario,
                     ssa::qt::toQString(source), ssa::qt::toQString(destination)});
        REQUIRE(child.waitForStarted(5000));
        QElapsedTimer deadline;
        deadline.start();
        const auto transactionJournal = std::filesystem::path{dbPath.string() + "-journal"};
        const auto checkpoint = committed ? readyPath : transactionJournal;
        while (!std::filesystem::exists(checkpoint) && deadline.elapsed() < 5000) {
            QThread::msleep(1);
        }
        REQUIRE(std::filesystem::exists(checkpoint));
        child.kill();
        REQUIRE(child.waitForFinished(5000));
        REQUIRE(child.exitStatus() == QProcess::CrashExit);
        if (!committed) {
            REQUIRE(sqlite3_finalize(readStatement) == SQLITE_OK);
            REQUIRE(sqlite3_exec(reader, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
            REQUIRE(sqlite3_close(reader) == SQLITE_OK);
        }

        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
        REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
        REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_import_consolidation_journal") ==
                (committed ? 0 : 1));
        REQUIRE(sqlite3_close(db) == SQLITE_OK);

        if (!committed) {
            writer.completeConsolidation({source});
        }
        REQUIRE(writer.pendingConsolidation().empty());
        ssa::infra::importing::ResolvedSsaImportRows retry;
        retry.rows.push_back({{"numero_ssa", "202600215"}, {"descricao_ssa", "Retry"}});
        REQUIRE(writer.write(retry, 1, 0, false).rowsWritten == 1);
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-journal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-wal"));
        REQUIRE_FALSE(std::filesystem::exists(dbPath.string() + "-shm"));
    }
}

TEST_CASE("canceling committed consolidation reports success and remains resumable") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto source = inputDirectory / "pending.xlsx";
    const auto destination = inputDirectory / "processadas" / "pending.xlsx";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    createSparseFile(source, 1);
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows rows;
    rows.rows.push_back({{"numero_ssa", "202600214"}, {"descricao_ssa", "Committed"}});
    auto session = writer.startSession(false);
    REQUIRE(session.write(rows, 1, 0).rowsWritten == 1);
    session.recordConsolidation({{{source, destination, true}}});
    static_cast<void>(session.finish());
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto canceled = port.importExternalFiles({.files = {source}}, stopSource.get_token());

    REQUIRE(canceled.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(canceled.warning);
    REQUIRE(canceled.message.find("canceled") != std::string::npos);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));
    REQUIRE(writer.pendingConsolidation().size() == 1);

    const auto resumed = port.importExternalFiles({.files = {source}});
    REQUIRE(resumed.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE_FALSE(resumed.warning);
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));
    REQUIRE(writer.pendingConsolidation().empty());
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
    writeWorkbook(workbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Setor Executor"), inlineCell("D1", "Descricao da SSA"),
                          inlineCell("E1", "Data de emissao")}) +
                      row(2, {inlineCell("A2", "202600001"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "MEL1"), inlineCell("D2", "Primeira importacao"),
                              inlineCell("E2", "2026-07-01")}));

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

TEST_CASE("external import preserves original filename and spreadsheet timestamp metadata") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "SSA_13-07-2026_0130PM.xlsx";
    writeWorkbook(source,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao"),
                          inlineCell("E1", "data_planilha")}) +
                      row(2, {inlineCell("A2", "202600501"), inlineCell("B2", "APV"),
                              inlineCell("C2", "Metadata"), inlineCell("D2", "2026-07-01"),
                              inlineCell("E2", "2026-07-13T13:20:00")}));
    setLocalModificationTime(source, 2026, 7, 13, 10, 15);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {source}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT arquivo_origem FROM ssa_table WHERE numero_ssa='202600501'") ==
            source.filename().string());
    REQUIRE(scalarText(db, "SELECT data_planilha FROM ssa_table WHERE numero_ssa='202600501'") ==
            "2026-07-13T13:20:00");
    REQUIRE(scalarText(db, "SELECT data_arquivo_origem FROM ssa_table "
                           "WHERE numero_ssa='202600501'") == "2026-07-13 10:15:00");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("external import falls back to the original source modification time") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "sem-data-no-nome.xlsx";
    writeWorkbook(source, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA"),
                                  inlineCell("D1", "Data de emissao")}) +
                              row(2, {inlineCell("A2", "202600502"), inlineCell("B2", "APV"),
                                      inlineCell("C2", "Mtime"), inlineCell("D2", "2026-07-01")}));
    setLocalModificationTime(source, 2026, 7, 12, 9, 45);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {source}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT arquivo_origem FROM ssa_table WHERE numero_ssa='202600502'") ==
            source.filename().string());
    REQUIRE(scalarText(db, "SELECT data_arquivo_origem FROM ssa_table "
                           "WHERE numero_ssa='202600502'") == "2026-07-12 09:45:00");
    REQUIRE(scalarText(db, "SELECT COALESCE(data_planilha, '') FROM ssa_table "
                           "WHERE numero_ssa='202600502'") == "");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet mapper keeps file modification time below spreadsheet time") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "metadata.xlsx";
    table.originalFilename = "metadata.xlsx";
    table.sourceModifiedTimestamp = "2026-07-12 09:45:00";
    table.rows = {{"Numero SSA", "Situacao", "Descricao da SSA", "Data de emissao"},
                  {"202600506", "APV", "Metadata", "2026-07-01"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(result.rows.size() == 1);
    REQUIRE(ssa::infra::importing::rowValue(result.rows.front(), "data_planilha").empty());
    REQUIRE(ssa::infra::importing::rowValue(result.rows.front(), "data_arquivo_origem") ==
            "2026-07-12 09:45:00");
}

TEST_CASE("external imports compare source times within the same day") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto older = sourceDirectory / "SSA_14-07-2026_0130PM.xlsx";
    const auto newer = sourceDirectory / "SSA_14-07-2026_0145PM.xlsx";
    const auto workbookRows = [](const std::string& description, const std::string& status) {
        return row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                       inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
               row(2, {inlineCell("A2", "202600507"), inlineCell("B2", status),
                       inlineCell("C2", description), inlineCell("D2", "2026-07-01")});
    };
    writeWorkbook(older, workbookRows("Older", "APV"));
    writeWorkbook(newer, workbookRows("Newer", "STE"));
    setLocalModificationTime(older, 2026, 7, 14, 13, 30);
    setLocalModificationTime(newer, 2026, 7, 14, 13, 45);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    REQUIRE(port.importExternalFiles({.files = {older}}).status ==
            ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(port.importExternalFiles({.files = {newer}}).status ==
            ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(port.importExternalFiles({.files = {older}}).status ==
            ssa::ports::WorkflowStatus::NoChanges);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600507'") ==
            "Newer");
    REQUIRE(scalarText(db, "SELECT situacao FROM ssa_table WHERE numero_ssa='202600507'") == "STE");
    REQUIRE(scalarText(db, "SELECT COALESCE(data_planilha, '') FROM ssa_table "
                           "WHERE numero_ssa='202600507'") == "");
    REQUIRE(scalarText(db, "SELECT data_arquivo_origem FROM ssa_table "
                           "WHERE numero_ssa='202600507'") == "2026-07-14 13:45:00");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("input staging keeps filename and local modification time separate") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto sourceDirectory = root / "source";
    std::filesystem::create_directories(sourceDirectory);
    const std::vector<std::string> cases{"SSA_14-07-2026_0343PM.xlsx",
                                         "SSA_14-07-2026 3:43 PM.xlsx", "SSA_2026_07_14T3.43.xlsx"};
    for (const auto& filename : cases) {
        const auto source = sourceDirectory / filename;
        createSparseFile(source, 1);
        setLocalModificationTime(source, 2000, 1, 2, 3, 4);
        const auto staged =
            ssa::infra::importing::ImportFileStager{inputDirectory}.stageExternalFiles({source});
        REQUIRE(staged.files.size() == 1);
        REQUIRE(staged.files.front().originalFilename == filename);
        REQUIRE(staged.files.front().sourceModifiedTimestamp == "2000-01-02 03:04:00");
#ifdef __APPLE__
        REQUIRE_FALSE(staged.files.front().sourceCreatedTimestamp.empty());
#endif
    }

    const auto unsupported = sourceDirectory / "SSA_2026-07-14.xlsx";
    createSparseFile(unsupported, 1);
    setLocalModificationTime(unsupported, 2000, 1, 2, 3, 4);
    const auto staged =
        ssa::infra::importing::ImportFileStager{inputDirectory}.stageExternalFiles({unsupported});
    REQUIRE(staged.files.size() == 1);
    REQUIRE(staged.files.front().sourceModifiedTimestamp == "2000-01-02 03:04:00");
}

TEST_CASE("spreadsheet mapper carries optional source creation time") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "created-time.xlsx";
    table.originalFilename = "created-time.xlsx";
    table.sourceCreatedTimestamp = "2026-07-14 13:45:00";
    table.rows = {{"Numero SSA", "Descricao da SSA", "Data de emissao"},
                  {"202600506", "Creation metadata", "2026-07-14 12:00:00"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(result.rows.size() == 1);
    REQUIRE(ssa::infra::importing::rowValue(result.rows.front(), "data_criacao_arquivo") ==
            "2026-07-14 13:45:00");
}

TEST_CASE("spreadsheet mapper accepts the real cadastro timestamp format") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "real-date.xlsx";
    table.rows = {{"Numero SSA", "Situacao", "Descricao da SSA", "Data de emissao"},
                  {"202600505", "APV", "Real timestamp", "21/10/2025 11:10:36"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(result.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(result.rows.size() == 1);
    REQUIRE(ssa::infra::importing::rowValue(result.rows.front(), "data_cadastro") ==
            "2025-10-21 11:10:36");
}

TEST_CASE("spreadsheet mapper prefers emission timestamp over issue timestamp") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "emission-priority.xlsx";
    table.rows = {
        {"Numero SSA", "Descricao da SSA", "emission_datetime", "issue_datetime", "Situacao"},
        {"202600508", "Emission priority", "2026-07-15 10:00:00", "2026-07-01 10:00:00", "APV"},
        {"202600509", "Issue fallback", "", "02/07/2026 10:00:00", "APV"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(result.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(result.rows.size() == 2);
    REQUIRE(ssa::infra::importing::rowValue(result.rows.front(), "data_cadastro") ==
            "2026-07-15 10:00:00");
    REQUIRE(ssa::infra::importing::rowValue(result.rows.back(), "data_cadastro") ==
            "2026-07-02 10:00:00");
}

TEST_CASE("spreadsheet mapper rejects exceptional rows without the date column") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "week-only.xlsx";
    table.rows = {{"Numero SSA", "Descricao da SSA", "Situacao", "Year Week"},
                  {"202600507", "Week only", "ASE", "202631"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper::map(table);

    REQUIRE(result.mappingStatus ==
            ssa::infra::importing::SpreadsheetMappingStatus::RequiredColumnsMissing);
    REQUIRE(result.rows.empty());
}

TEST_CASE("external import rejects a workbook without the required date column") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "schema-incompleto.xlsx";
    writeWorkbook(source, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                  inlineCell("C1", "Descricao da SSA")}) +
                              row(2, {inlineCell("A2", "202600503"), inlineCell("B2", "ASE"),
                                      inlineCell("C2", "Sem coluna de data")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {source}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("required_columns_missing") != std::string::npos);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
                          "name='ssa_table'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("SSA duplicate resolution is temporal and independent of input order") {
    const ssa::infra::importing::SsaImportRow older{{"numero_ssa", "202600504"},
                                                    {"situacao", "APV"},
                                                    {"descricao_ssa", "Older"},
                                                    {"data_arquivo_origem", "2026-07-13 09:00:00"}};
    const ssa::infra::importing::SsaImportRow newer{{"numero_ssa", "202600504"},
                                                    {"situacao", "STE"},
                                                    {"descricao_ssa", "Newer"},
                                                    {"data_arquivo_origem", "2026-07-13 14:30:00"}};
    const auto resolve = [](const ssa::infra::importing::SsaImportRow& first,
                            const ssa::infra::importing::SsaImportRow& second) {
        ssa::infra::importing::SsaImportBatch firstBatch;
        firstBatch.rows = {first};
        ssa::infra::importing::SsaImportBatch secondBatch;
        secondBatch.rows = {second};
        return ssa::infra::importing::SsaImportConflictResolver{}
            .resolveBySsaNumberKeepingUnkeyedRows({firstBatch, secondBatch});
    };

    const auto oldThenNew = resolve(older, newer);
    const auto newThenOld = resolve(newer, older);

    REQUIRE(oldThenNew.rows.size() == 1);
    REQUIRE(newThenOld.rows.size() == 1);
    REQUIRE(oldThenNew.duplicateRows == 1);
    REQUIRE(newThenOld.duplicateRows == 1);
    REQUIRE(ssa::infra::importing::rowValue(oldThenNew.rows.front(), "situacao") == "STE");
    REQUIRE(ssa::infra::importing::rowValue(newThenOld.rows.front(), "situacao") == "STE");
    REQUIRE(ssa::infra::importing::rowValue(oldThenNew.rows.front(), "descricao_ssa") == "Newer");
    REQUIRE(oldThenNew.rows == newThenOld.rows);

    const auto equalDuplicate = resolve(newer, newer);
    REQUIRE(equalDuplicate.rows == std::vector<ssa::infra::importing::SsaImportRow>{newer});
    REQUIRE(equalDuplicate.duplicateRows == 1);
}

TEST_CASE("SSA duplicate resolution rejects conflicting values at the same snapshot") {
    ssa::infra::importing::SsaImportBatch batch;
    batch.rows = {{{"numero_ssa", "202600508"},
                   {"descricao_ssa", "First"},
                   {"situacao", "APV"},
                   {"data_planilha", "2026-07-14T13:45:00"}},
                  {{"numero_ssa", "202600508"},
                   {"descricao_ssa", "Second"},
                   {"situacao", "APV"},
                   {"data_planilha", "2026-07-14T13:45:00"}}};

    const auto result =
        ssa::infra::importing::SsaImportConflictResolver{}.resolveBySsaNumberKeepingUnkeyedRows(
            {batch});

    REQUIRE(result.rows.empty());
}

TEST_CASE("external import rejects duplicate conflicts and preserves the source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto source = sourceDirectory / "SSA_14-07-2026_0145PM.xlsx";
    writeWorkbook(
        source,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600509"), inlineCell("B2", "APV"),
                    inlineCell("C2", "SECRET_FIRST"), inlineCell("D2", "2026-07-01")}) +
            row(3, {inlineCell("A3", "202600509"), inlineCell("B3", "APV"),
                    inlineCell("C3", "SECRET_SECOND"), inlineCell("D3", "2026-07-01")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {source}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("duplicate_conflict") != std::string::npos);
    REQUIRE(result.message.find("conflicts=1") != std::string::npos);
    REQUIRE(result.message.find("SECRET_") == std::string::npos);
    REQUIRE(result.diagnostic.find("SECRET_") == std::string::npos);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
                          "name='ssa_table'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("external import rejects equal snapshot conflicts across files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "external";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto first = sourceDirectory / "A_SSA_14-07-2026_0145PM.xlsx";
    const auto second = sourceDirectory / "B_SSA_14-07-2026_0145PM.xlsx";
    const auto workbookRows = [](const std::string& description) {
        return row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                       inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
               row(2, {inlineCell("A2", "202600510"), inlineCell("B2", "APV"),
                       inlineCell("C2", description), inlineCell("D2", "2026-07-01")});
    };
    writeWorkbook(first, workbookRows("FIRST_FILE"));
    writeWorkbook(second, workbookRows("SECOND_FILE"));
    setLocalModificationTime(first, 2026, 7, 14, 13, 45);
    setLocalModificationTime(second, 2026, 7, 14, 13, 45);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {first, second}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("duplicate_conflict") != std::string::npos);
    REQUIRE(std::filesystem::exists(first));
    REQUIRE(std::filesystem::exists(second));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
                          "name='ssa_table'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("incremental rescan rejects an unrecognized workbook without moving it") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "unknown.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Unknown A"), inlineCell("B1", "Unknown B"),
                                    inlineCell("C1", "Unknown C")}));

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600130"}, {"descricao_ssa", "Existing"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("header_not_recognized") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600130'") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet mapper rejects invalid rows and accepts date exempt states") {
    ssa::infra::importing::SpreadsheetTable table;
    table.sourcePath = "rows.xlsx";
    table.rows = {
        {"Numero SSA", "Situacao", "Descricao da SSA", "Data de emissao", "Semana cadastro"},
        {"SSA 202600001", "APV", "Invalid number", "2026-01-01"},
        {"2026-00002", "APV", "Missing date", ""},
        {"2026-00003", "ASE", "Exempt date", "", "202601"},
        {"202600004.0", "APV", "Valid", "2026-01-01"}};

    const auto result = ssa::infra::importing::SsaSpreadsheetMapper{}.map(table);

    REQUIRE(result.mappingStatus == ssa::infra::importing::SpreadsheetMappingStatus::Mapped);
    REQUIRE(result.rows.size() == 2);
    REQUIRE(result.skippedRows == 2);
    REQUIRE(ssa::infra::importing::rowValue(result.rows[0], "numero_ssa") == "202600003");
    REQUIRE(ssa::infra::importing::rowValue(result.rows[1], "numero_ssa") == "202600004");
}

TEST_CASE("spreadsheet workflow reports invalid row causes without cell content") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "rows.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600510"), inlineCell("B2", "APV"),
                    inlineCell("C2", "SECRET_ROW_PAYLOAD"), inlineCell("D2", "not-a-date")}) +
            row(3, {inlineCell("A3", "202600511"), inlineCell("B3", "APV"),
                    inlineCell("C3", "Valid"), inlineCell("D3", "2026-07-01")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.message.find("invalid_rows=1") != std::string::npos);
    REQUIRE(result.message.find("invalid_number=0") != std::string::npos);
    REQUIRE(result.message.find("invalid_description=0") != std::string::npos);
    REQUIRE(result.message.find("invalid_date=1") != std::string::npos);
    REQUIRE(result.message.find("skipped=1") != std::string::npos);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->skippedRows == 1);
    REQUIRE(result.message.find("SECRET_ROW_PAYLOAD") == std::string::npos);
    REQUIRE(result.diagnostic.find("SECRET_ROW_PAYLOAD") == std::string::npos);
    REQUIRE(result.warning);
}

TEST_CASE("sqlite incremental import merges without deleting existing fields") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600131"},
                             {"descricao_ssa", "Existing"},
                             {"setor_executor", "MEL1"},
                             {"situacao", "STE"},
                             {"data_planilha", "2026-05-01"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600131"},
                             {"descricao_ssa", "Regressed"},
                             {"setor_executor", ""},
                             {"situacao", "APV"},
                             {"prazo_limite", "2026-05-30"},
                             {"data_planilha", "2026-05-02"}});

    const auto summary = writer.write(incoming, 1, 0, false);

    REQUIRE(summary.rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600131'") ==
            "Existing");
    REQUIRE(scalarText(db, "SELECT setor_executor FROM ssa_table WHERE numero_ssa='202600131'") ==
            "MEL1");
    REQUIRE(scalarText(db, "SELECT situacao FROM ssa_table WHERE numero_ssa='202600131'") == "STE");
    REQUIRE(scalarText(db, "SELECT prazo_limite FROM ssa_table WHERE numero_ssa='202600131'") ==
            "2026-05-30");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("incremental no-op reports a successful non-mutating outcome") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "same.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600132"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Unchanged"), inlineCell("D2", "2026-07-01")}));
    setLocalModificationTime(workbook, 2026, 7, 14, 12, 0);

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600132"},
                             {"situacao", "ASE"},
                             {"descricao_ssa", "Unchanged"},
                             {"data_cadastro", "2026-07-01"},
                             {"arquivo_origem", "same.xlsx"},
                             {"data_arquivo_origem", "2026-07-14 12:00:00"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.ok());
    REQUIRE(result.status != ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.message.find("inserted=0") != std::string::npos);
    REQUIRE(result.message.find("updated=0") != std::string::npos);
    REQUIRE(result.message.find("unchanged=1") != std::string::npos);
    REQUIRE(std::filesystem::exists(inputDirectory / "processadas" / "same.xlsx"));
}

TEST_CASE("incremental rescan rejects an empty recognized workbook and preserves its source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto workbook = inputDirectory / "empty.xlsx";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    writeWorkbook(workbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao da SSA"),
                          inlineCell("C1", "Data de emissao")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("no_valid_rows") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas" / "nosurvivor" /
                                          workbook.filename()));
}

TEST_CASE("incremental no-op reports success when post commit consolidation is interrupted") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "same.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600216"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Unchanged"), inlineCell("D2", "2026-07-01")}));
    setLocalModificationTime(workbook, 2026, 7, 14, 12, 0);
    createSparseFile(inputDirectory / "processadas", 1);

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600216"},
                             {"situacao", "ASE"},
                             {"descricao_ssa", "Unchanged"},
                             {"data_cadastro", "2026-07-01"},
                             {"arquivo_origem", "same.xlsx"},
                             {"data_arquivo_origem", "2026-07-14 12:00:00"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.message.find("inserted=0") != std::string::npos);
    REQUIRE(result.message.find("updated=0") != std::string::npos);
    REQUIRE(result.message.find("unchanged=1") != std::string::npos);
    REQUIRE(result.message.find("error=consolidation_failed") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(writer.pendingConsolidation().empty());
}

TEST_CASE("sqlite writer maintains an indexed derived count summary") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "summary.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows rows;
    rows.rows = {
        {{"numero_ssa", "202600600"},
         {"situacao", "APV"},
         {"descricao_ssa", "Parent"},
         {"data_cadastro", "2026-07-14"}},
        {{"numero_ssa", "202600601"},
         {"situacao", "APV"},
         {"descricao_ssa", "Child one"},
         {"data_cadastro", "2026-07-14"},
         {"derivada_de", "202600600"}},
        {{"numero_ssa", "202600602"},
         {"situacao", "APV"},
         {"descricao_ssa", "Child two"},
         {"data_cadastro", "2026-07-14"},
         {"derivada_de", "202600600"}},
    };
    REQUIRE(writer.write(rows, 1, 0, false).rowsWritten == 3);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT qtd_derivadas FROM ssa_table_derived_counts WHERE "
                          "parent_ssa='202600600'") == 2);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET derivada_de='202600999' WHERE "
                         "numero_ssa='202600602'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT qtd_derivadas FROM ssa_table_derived_counts WHERE "
                          "parent_ssa='202600600'") == 1);
    REQUIRE(scalarInt(db, "SELECT qtd_derivadas FROM ssa_table_derived_counts WHERE "
                          "parent_ssa='202600999'") == 1);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET derivada_de='' WHERE "
                         "numero_ssa='202600602'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table_derived_counts WHERE parent_ssa=''") ==
            0);
    REQUIRE(sqlite3_exec(db, "DELETE FROM ssa_table WHERE numero_ssa='202600601'", nullptr, nullptr,
                         nullptr) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COALESCE((SELECT qtd_derivadas FROM "
                          "ssa_table_derived_counts WHERE parent_ssa='202600600'), 0)") == 0);
    REQUIRE(sqlite3_exec(db,
                         "INSERT OR REPLACE INTO ssa_table_derived_counts(parent_ssa, "
                         "qtd_derivadas) VALUES('202600600', 7)",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM sqlite_schema WHERE type='table' AND "
                          "name='ssa_table_derived_counts'") == 1);
    REQUIRE(scalarInt(db, "SELECT COALESCE(d.qtd_derivadas, 0) FROM ssa_table AS s LEFT JOIN "
                          "ssa_table_derived_counts AS d ON d.parent_ssa=TRIM(COALESCE("
                          "s.numero_ssa, '')) WHERE s.numero_ssa='202600600'") == 7);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::sqlite::SqliteSsaRepository repository(dbPath);
    ssa::domain::SsaPageRequest request;
    request.visibleColumns = {"numero_ssa", "qtd_derivadas"};
    request.pageSize = 10;
    request.excludeScaSesSte = false;
    const auto page = repository.page(request);
    const auto parent = std::ranges::find_if(
        page.rows, [](const auto& row) { return row.valueOf("numero_ssa") == "202600600"; });
    REQUIRE(parent != page.rows.end());
    REQUIRE(parent->valueOf("qtd_derivadas") == "7");
}

TEST_CASE("incremental summary reports transactional legacy normalization separately from file") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "same-normalized.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600526"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Unchanged"), inlineCell("D2", "2026-07-01")}));
    setLocalModificationTime(workbook, 2026, 7, 14, 12, 0);

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600526"},
                             {"situacao", "ASE"},
                             {"descricao_ssa", "Unchanged"},
                             {"data_cadastro", "2026-07-01"},
                             {"arquivo_origem", "same-normalized.xlsx"},
                             {"data_arquivo_origem", "2026-07-14 12:00:00"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET numero_ssa='2026-00526.0' WHERE "
                         "numero_ssa='202600526'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->updates == 1);
    REQUIRE(result.importSummary->files.size() == 1);
    REQUIRE(result.importSummary->files.front().status == ssa::ports::ImportFileStatus::NoChanges);
    REQUIRE(result.importSummary->files.front().updates == 0);
    REQUIRE(result.message.find("updated=1") != std::string::npos);
}

TEST_CASE("sqlite import rejects duplicate existing SSA numbers before mutation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacyRow;
    legacyRow.rows.push_back({{"numero_ssa", "202600133"}, {"descricao_ssa", "First"}});
    REQUIRE(writer.write(legacyRow, 1, 0, false).rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "DROP INDEX ux_ssa_table_numero_ssa", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    REQUIRE(sqlite3_exec(
                db,
                "INSERT INTO ssa_table(numero_ssa, descricao_ssa) VALUES('202600133', 'Second')",
                nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600134"}, {"descricao_ssa", "Must not enter"}});

    REQUIRE_THROWS(writer.write(incoming, 1, 0, false));

    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600133'") == 2);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600134'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import rejects invalid integer values before mutation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows seed;
    seed.rows.push_back({{"numero_ssa", "202600134"}, {"descricao_ssa", "Existing"}});
    REQUIRE(writer.write(seed, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600135"},
                             {"descricao_ssa", "Invalid week"},
                             {"semana_cadastro", "not-an-integer"}});

    REQUIRE_THROWS(writer.write(incoming, 1, 0, false));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600134'") ==
            "Existing");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import rejects semantic SSA collisions before mutation") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows = {{{"numero_ssa", "202600512"}, {"descricao_ssa", "First"}},
                   {{"numero_ssa", "202600599"}, {"descricao_ssa", "Second"}}};
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 2);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "DROP INDEX ux_ssa_table_numero_ssa", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET numero_ssa='2026-00512' WHERE "
                         "descricao_ssa='First'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET numero_ssa='202600512.0' WHERE "
                         "descricao_ssa='Second'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600513"}, {"descricao_ssa", "Must not enter"}});

    REQUIRE_THROWS(writer.write(incoming, 1, 0, false));

    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 2);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600513'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import normalizes unique legacy SSA values transactionally") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows = {{{"numero_ssa", "202600514"}, {"descricao_ssa", "Legacy"}},
                   {{"numero_ssa", "202600515"},
                    {"descricao_ssa", "Child"},
                    {"derivada_de", "202600514"},
                    {"numero_ssa_relacionada_1", "202600514"}}};
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 2);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET numero_ssa='2026-00514.0' WHERE "
                         "numero_ssa='202600514'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET derivada_de='2026-00514.0', "
                         "numero_ssa_relacionada_1='2026-00514.0' WHERE "
                         "numero_ssa='202600515'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600516"}, {"descricao_ssa", "Incoming"}});
    const auto summary = writer.write(incoming, 1, 0, false);
    REQUIRE(summary.rowsWritten == 3);
    REQUIRE(summary.rowsUpdated == 2);

    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600514'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='2026-00514.0'") == 0);
    REQUIRE(scalarText(db, "SELECT derivada_de FROM ssa_table WHERE numero_ssa='202600515'") ==
            "202600514");
    REQUIRE(scalarText(db, "SELECT numero_ssa_relacionada_1 FROM ssa_table WHERE "
                           "numero_ssa='202600515'") == "202600514");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite full import replaces exact duplicate legacy identities") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows.push_back({{"numero_ssa", "202600519"}, {"descricao_ssa", "First"}});
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "DROP INDEX ux_ssa_table_numero_ssa", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    REQUIRE(sqlite3_exec(
                db,
                "INSERT INTO ssa_table(numero_ssa, descricao_ssa) VALUES('202600519', 'Second')",
                nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600520"}, {"descricao_ssa", "Replacement"}});
    const auto summary = writer.write(replacement, 1, 0, true);

    REQUIRE(summary.rowsWritten == 1);
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600520'") == 1);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import normalizes formatted references to canonical existing identities") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows = {{{"numero_ssa", "202600521"}, {"descricao_ssa", "Parent"}},
                   {{"numero_ssa", "202600522"},
                    {"descricao_ssa", "Child"},
                    {"derivada_de", "202600521"},
                    {"numero_ssa_relacionada_1", "202600521"}}};
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 2);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET derivada_de='2026-00521.0', "
                         "numero_ssa_relacionada_1='2026-00521.0' WHERE "
                         "numero_ssa='202600522'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows.push_back({{"numero_ssa", "202600523"}, {"descricao_ssa", "Incoming"}});
    const auto summary = writer.write(incoming, 1, 0, false);

    REQUIRE(summary.rowsWritten == 2);
    REQUIRE(summary.rowsUpdated == 1);
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT derivada_de FROM ssa_table WHERE numero_ssa='202600522'") ==
            "202600521");
    REQUIRE(scalarText(db, "SELECT numero_ssa_relacionada_1 FROM ssa_table WHERE "
                           "numero_ssa='202600522'") == "202600521");
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import normalizes formatted references before inserting rows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows incoming;
    incoming.rows = {{{"numero_ssa", "202600524"}, {"descricao_ssa", "Parent"}},
                     {{"numero_ssa", "202600525"},
                      {"descricao_ssa", "Child"},
                      {"derivada_de", "2026-00524.0"},
                      {"numero_ssa_relacionada_1", "2026-00524.0"}}};

    const auto summary = writer.write(incoming, 1, 0, false);

    REQUIRE(summary.rowsWritten == 2);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "SELECT derivada_de FROM ssa_table WHERE numero_ssa='202600525'") ==
            "202600524");
    REQUIRE(scalarText(db, "SELECT numero_ssa_relacionada_1 FROM ssa_table WHERE "
                           "numero_ssa='202600525'") == "202600524");
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite full import replaces an invalid legacy database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows.push_back({{"numero_ssa", "202600517"}, {"descricao_ssa", "Legacy"}});
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "UPDATE ssa_table SET numero_ssa='invalid-legacy'", nullptr, nullptr,
                         nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    ssa::infra::importing::ResolvedSsaImportRows replacement;
    replacement.rows.push_back({{"numero_ssa", "202600518"}, {"descricao_ssa", "Replacement"}});
    const auto summary = writer.write(replacement, 1, 0, true);

    REQUIRE(summary.rowsWritten == 1);
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600518'") == 1);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("sqlite import rolls back legacy SSA normalization when preflight fails") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto dbPath = std::filesystem::path{tempDir.path().toStdString()} / "ssas.db";
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows legacy;
    legacy.rows.push_back({{"numero_ssa", "202600516"}, {"descricao_ssa", "Legacy"}});
    REQUIRE(writer.write(legacy, 1, 0, false).rowsWritten == 1);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "UPDATE ssa_table SET numero_ssa='2026-00516.0' WHERE "
                         "numero_ssa='202600516'",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "CREATE TRIGGER reject_normalization BEFORE UPDATE OF numero_ssa ON "
                         "ssa_table WHEN NEW.numero_ssa='202600516' BEGIN SELECT "
                         "RAISE(ABORT, 'test normalization failure'); END",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    REQUIRE_THROWS(writer.startSession(false));

    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='2026-00516.0'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600516'") == 0);
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
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    const auto validWorkbook = inputDirectory / "valid.xlsx";
    writeWorkbook(
        validWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600120"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Linha valida"), inlineCell("D2", "2026-07-01")}));
    const auto emptyWorkbook = inputDirectory / "empty.xlsx";
    writeWorkbook(
        emptyWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}));
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
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600121"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Novo arquivo"), inlineCell("D2", "2026-07-01")}));

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
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600124"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Commit antes do move"), inlineCell("D2", "2026-07-01")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.warning);
    REQUIRE(result.message.find("error=consolidation_failed") != std::string::npos);
    REQUIRE(result.diagnostic.find("cannot create consolidation directory") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE(std::filesystem::exists(dbPath));
    const ssa::infra::sqlite::SqliteSsaImportWriter publishedWriter(dbPath, importColumns());
    REQUIRE_FALSE(publishedWriter.pendingConsolidation().empty());

    REQUIRE(std::filesystem::remove(processedPath));
    REQUIRE(std::filesystem::create_directory(processedPath));
    const auto resumed = port.rescan({ssa::ports::RescanMode::Incremental});
    REQUIRE(resumed.ok());
    REQUIRE_FALSE(resumed.warning);
    REQUIRE_FALSE(std::filesystem::exists(workbook));
    REQUIRE(std::filesystem::exists(processedPath / workbook.filename()));
    REQUIRE(publishedWriter.pendingConsolidation().empty());
}

TEST_CASE("rescan publishes from a protected copy while the original is read locked") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600125"}, {"descricao_ssa", "Linha anterior"}});
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
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600126"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Nao consolidar"), inlineCell("D2", "2026-07-01")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(sqlite3_finalize(lockedStatement) == SQLITE_OK);
    REQUIRE(sqlite3_exec(readLock, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(readLock) == SQLITE_OK);
    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE_FALSE(result.warning);
    REQUIRE_FALSE(std::filesystem::exists(workbook));
    REQUIRE(std::filesystem::exists(inputDirectory / "processadas" / workbook.filename()));
    sqlite3* verificationDb = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &verificationDb) == SQLITE_OK);
    REQUIRE(scalarInt(verificationDb,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600125'") == 1);
    REQUIRE(scalarInt(verificationDb,
                      "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600126'") == 1);
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
    writeWorkbook(
        source,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600122"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Import externo"), inlineCell("D2", "2026-07-01")}));

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
    REQUIRE(std::filesystem::exists(processedDirectory / source.filename()));
}

TEST_CASE("spreadsheet import workflow preserves unicode paths") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto asciiWorkbook =
        std::filesystem::path{tempDir.path().toStdString()} / "unicode-source.xlsx";
    writeWorkbook(
        asciiWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600099"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Caminho unicode"), inlineCell("D2", "2026-07-01")}));

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
    writeWorkbook(workbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao"),
                          inlineCell("E1", "Setor Executor")}) +
                      row(2, {inlineCell("A2", "202600777"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Linha esparsa"), inlineCell("D2", "2026-07-01"),
                              inlineCell("E2", "MEL9")}));

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

TEST_CASE("spreadsheet import workflow processes more than 64 discovered files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    for (std::size_t index = 0; index < 65; ++index) {
        const auto workbook = inputDirectory / ("source_" + std::to_string(index) + ".xlsx");
        writeWorkbook(
            workbook,
            row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                    inlineCell("C1", "Data Cadastro")}) +
                row(2, {inlineCell("A2", std::to_string(202600000 + index)),
                        inlineCell("B2", "Windowed import"), inlineCell("C2", "2026-07-14")}));
    }
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    INFO(result.diagnostic);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 65);
    REQUIRE(result.importSummary->inserts == 65);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 65);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet import workflow reports missing selected file metadata as failed") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {root / "missing.xlsx"}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("file_size_unavailable") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
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

TEST_CASE("import file stager rejects more than 64 externally selected files") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "source";
    const auto inputDirectory = root / "docs_entrada";
    std::filesystem::create_directories(sourceDirectory);
    std::vector<std::filesystem::path> files;
    for (std::size_t index = 0; index < 65; ++index) {
        const auto source = sourceDirectory / ("source_" + std::to_string(index) + ".xlsx");
        createSparseFile(source, 0);
        files.push_back(source);
    }

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const auto result = stager.stageExternalFiles(files);

    REQUIRE(result.rejectionReason == "too_many_files max=64");
    REQUIRE(result.files.empty());
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory));
}

TEST_CASE("input file stager inventories 1769 xlsx files without a quantity rejection") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    for (std::size_t index = 0; index < 1'769; ++index) {
        createSparseFile(inputDirectory / ("source_" + std::to_string(index) + ".xlsx"), 0);
    }

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const auto result = stager.stageInputFiles();

    REQUIRE(result.rejectionReason.empty());
    REQUIRE(result.discoveredXlsxSources.size() == 1'769);
    REQUIRE(result.files.size() == 1'769);
}

TEST_CASE("input file stager removes abandoned owned staging artifacts") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    std::filesystem::create_directories(inputDirectory);
    createSparseFile(inputDirectory / ".ssa-staged-crashed_123_0.xlsx", 1);
    createSparseFile(inputDirectory / "pending.xlsx", 1);

    const ssa::infra::importing::ImportFileStager stager(inputDirectory);
    const auto result = stager.stageInputFiles();

    REQUIRE(result.rejectionReason.empty());
    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files.front().workbookPath == inputDirectory / "pending.xlsx");
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / ".ssa-staged-crashed_123_0.xlsx"));
}

#ifndef _WIN32
TEST_CASE("input file stager reports an abandoned staging artifact that cannot be removed") {
    if (::geteuid() == 0) {
        SKIP("permission cleanup failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputDirectory =
        std::filesystem::path{tempDir.path().toStdString()} / "docs_entrada";
    const auto abandoned = inputDirectory / ".ssa-staged-crashed_123_0.xlsx";
    std::filesystem::create_directories(inputDirectory);
    createSparseFile(abandoned, 1);
    std::filesystem::permissions(
        inputDirectory, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
    const ssa::infra::importing::ImportFileStager stager(inputDirectory);

    const auto result = stager.stageInputFiles();

    std::filesystem::permissions(inputDirectory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    REQUIRE(result.rejectionReason == "staging_cleanup_failed");
    REQUIRE(result.diagnostic.find("operation=remove_abandoned_staging") != std::string::npos);
    REQUIRE(result.diagnostic.find("path=" + abandoned.string()) != std::string::npos);
    REQUIRE(result.diagnostic.find("error=") != std::string::npos);
    REQUIRE(result.diagnostic.find("pending=true") != std::string::npos);
    REQUIRE(std::filesystem::exists(abandoned));
}
#endif

TEST_CASE("spreadsheet import workflow rejects a second instance before discovery") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    QLockFile heldLock(ssa::qt::toQString(root / ".ssa_import.lock"));
    heldLock.setStaleLockTime(0);
    REQUIRE(heldLock.tryLock(0));
    const auto workbook = inputDirectory / "pending.xlsx";
    std::filesystem::create_directories(inputDirectory);
    createSparseFile(workbook, 0);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("import_already_running") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("spreadsheet import rejects a second input root targeting the same database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto firstInput = root / "first" / "docs_entrada";
    const auto secondInput = root / "second" / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(firstInput);
    std::filesystem::create_directories(secondInput);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto databaseLockPath = databaseImportLockPathForTest(dbPath);
    QLockFile heldLock(ssa::qt::toQString(databaseLockPath));
    heldLock.setStaleLockTime(0);
    REQUIRE(heldLock.tryLock(0));
    createSparseFile(secondInput / "pending.xlsx", 0);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(secondInput, dbPath, importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("import_already_running") != std::string::npos);
    REQUIRE(std::filesystem::exists(secondInput / "pending.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("spreadsheet import lock resolves database symlink aliases") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "input" / "docs_entrada";
    const auto databasePath = root / "data" / "ssas.db";
    const auto aliasPath = root / "aliases" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(databasePath.parent_path());
    std::filesystem::create_directories(aliasPath.parent_path());
    std::ofstream{databasePath}.close();
    std::error_code error;
    std::filesystem::create_symlink(databasePath, aliasPath, error);
    if (error) {
        SKIP("symbolic links are unavailable in this environment");
    }

    QLockFile heldLock(ssa::qt::toQString(databaseImportLockPathForTest(databasePath)));
    heldLock.setStaleLockTime(0);
    REQUIRE(heldLock.tryLock(0));
    createSparseFile(inputDirectory / "pending.xlsx", 0);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, aliasPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("import_already_running") != std::string::npos);
    REQUIRE(std::filesystem::exists(inputDirectory / "pending.xlsx"));
}

TEST_CASE("spreadsheet import recreates a renamed database without deleting the backup") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto backupPath = root / "data" / "ssas.db.before-recreate";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    writeWorkbook(inputDirectory / "first.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                          inlineCell("C1", "Data Cadastro")}) +
                      row(2, {inlineCell("A2", "202600701"), inlineCell("B2", "First"),
                              inlineCell("C2", "2026-07-15")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto first = port.rescan({ssa::ports::RescanMode::Incremental});
    REQUIRE(first.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::exists(dbPath));

    std::filesystem::rename(dbPath, backupPath);
    REQUIRE(std::filesystem::exists(backupPath));
    writeWorkbook(inputDirectory / "second.xlsx",
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                          inlineCell("C1", "Data Cadastro")}) +
                      row(2, {inlineCell("A2", "202600702"), inlineCell("B2", "Second"),
                              inlineCell("C2", "2026-07-15")}));

    const auto second = port.rescan({ssa::ports::RescanMode::Incremental});
    REQUIRE(second.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(std::filesystem::exists(dbPath));

    const ssa::infra::sqlite::SqliteDatabaseValidator validator;
    REQUIRE(validator.validate(backupPath).status == ssa::ports::DatabaseValidationStatus::Valid);
    REQUIRE(validator.validate(dbPath).status == ssa::ports::DatabaseValidationStatus::Valid);

    sqlite3* backup = nullptr;
    sqlite3* recreated = nullptr;
    REQUIRE(sqlite3_open(backupPath.string().c_str(), &backup) == SQLITE_OK);
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &recreated) == SQLITE_OK);
    REQUIRE(scalarInt(backup, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600701'") == 1);
    REQUIRE(scalarInt(recreated, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600701'") ==
            0);
    REQUIRE(scalarInt(recreated, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600702'") ==
            1);
    REQUIRE(sqlite3_close(backup) == SQLITE_OK);
    REQUIRE(sqlite3_close(recreated) == SQLITE_OK);
}

TEST_CASE("external import lock failure preserves the selected file inventory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "alternate.sqlite";
    const auto first = root / "first.xlsx";
    const auto second = root / "second.xlsx";
    createSparseFile(first, 0);
    createSparseFile(second, 0);
    QLockFile heldLock(ssa::qt::toQString(root / ".ssa_import.lock"));
    heldLock.setStaleLockTime(0);
    REQUIRE(heldLock.tryLock(0));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importExternalFiles({.files = {first, second}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message == "import_already_running");
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 2);
    REQUIRE(result.importSummary->rejected == 2);
    REQUIRE(result.importSummary->preserved == 2);
    REQUIRE(result.importSummary->files.size() == 2);
    REQUIRE(result.importSummary->files[0].source == "first.xlsx");
    REQUIRE(result.importSummary->files[0].status == ssa::ports::ImportFileStatus::Failed);
    REQUIRE(result.importSummary->files[1].source == "second.xlsx");
    REQUIRE(result.importSummary->files[1].status == ssa::ports::ImportFileStatus::Failed);
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

#ifndef _WIN32
TEST_CASE("external import lock permission failure includes a technical diagnostic") {
    if (::geteuid() == 0) {
        SKIP("permission lock failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "alternate.sqlite";
    const auto source = root / "selected.xlsx";
    createSparseFile(source, 0);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::filesystem::permissions(
        root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    const auto result = port.importExternalFiles({.files = {source}});

    std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message == "import_lock_failed");
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 1);
    REQUIRE(result.importSummary->preserved == 1);
    REQUIRE(result.importSummary->files.front().source == "selected.xlsx");
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}
#endif

TEST_CASE("spreadsheet import workflow holds the corpus lock until an alternate database import "
          "finishes") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "alternate.sqlite";
    const auto importLockPath = root / ".ssa_import.lock";
    std::filesystem::create_directories(inputDirectory);
    const auto workbook = inputDirectory / "pending.xlsx";
    writeWorkbook(workbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                          inlineCell("C1", "Data Cadastro")}) +
                      row(2, {inlineCell("A2", "202600901"), inlineCell("B2", "Lock lifetime"),
                              inlineCell("C2", "2026-07-14")}));
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write({}, 0, 0, false).rowsWritten == 0);
    ssa::infra::sqlite::SqliteConnection blocker(dbPath,
                                                 ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    REQUIRE(sqlite3_exec(blocker.handle(), "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
            SQLITE_OK);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    auto operation = std::async(std::launch::async,
                                [&] { return port.rescan({ssa::ports::RescanMode::Incremental}); });
    QElapsedTimer deadline;
    deadline.start();
    while (!std::filesystem::exists(importLockPath) && deadline.elapsed() < 3'000) {
        QThread::msleep(5);
    }
    REQUIRE(std::filesystem::exists(importLockPath));
    REQUIRE(operation.wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);
    QLockFile contender(ssa::qt::toQString(importLockPath));
    contender.setStaleLockTime(0);
    REQUIRE_FALSE(contender.tryLock(0));

    REQUIRE(sqlite3_exec(blocker.handle(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(operation.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    const auto result = operation.get();

    INFO(result.message);
    INFO(result.diagnostic);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(contender.tryLock(0));
    contender.unlock();
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
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600010"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Linha nova"), inlineCell("D2", "2026-07-01")}));

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

TEST_CASE("full rescan leaves legacy XLS pending and imports valid XLSX") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back(
        {{"numero_ssa", "202600399"}, {"situacao", "ASE"}, {"descricao_ssa", "Anterior"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    const auto legacy = inputDirectory / "pending.xls";
    {
        std::ofstream stream(legacy);
        stream << "legacy workbook";
    }
    const auto workbook = inputDirectory / "valid.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA"),
                                    inlineCell("D1", "Data de emissao")}) +
                                row(2, {inlineCell("A2", "202600400"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Nova"), inlineCell("D2", "2026-07-01")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.ok());
    REQUIRE(std::filesystem::exists(legacy));
    REQUIRE(std::filesystem::exists(inputDirectory / "processadas" / "valid.xlsx"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600399'") == 0);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600400'") == 1);
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
    writeWorkbook(
        seedWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600126"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Linha preservada"), inlineCell("D2", "2026-07-01")}));
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
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->discovered == 1);
    REQUIRE(result.importSummary->pending == 1);
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

TEST_CASE("incremental import exposes truthful per-file and aggregate summary") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto appliedWorkbook = inputDirectory / "a-applied.xlsx";
    writeWorkbook(
        appliedWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600601"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Applied"), inlineCell("D2", "2026-07-01")}) +
            row(3, {inlineCell("A3", "invalid"), inlineCell("B3", "ASE"),
                    inlineCell("C3", "Invalid"), inlineCell("D3", "2026-07-01")}));
    const auto emptyWorkbook = inputDirectory / "b-empty.xlsx";
    writeWorkbook(
        emptyWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.importSummary.has_value());
    const auto& summary = *result.importSummary;
    REQUIRE(summary.discovered == 2);
    REQUIRE(summary.accepted == 2);
    REQUIRE(summary.rejected == 0);
    REQUIRE(summary.pending == 0);
    REQUIRE(summary.preserved == 0);
    REQUIRE(summary.validRows == 1);
    REQUIRE(summary.invalidRows == 1);
    REQUIRE(summary.inserts == 1);
    REQUIRE(summary.updates == 0);
    REQUIRE(summary.unchangedRows == 0);
    REQUIRE(summary.conflicts == 0);
    REQUIRE(summary.consolidated == 1);
    REQUIRE(summary.noSurvivor == 1);
    REQUIRE(summary.files.size() == 2);
    REQUIRE(summary.files[0].source == "a-applied.xlsx");
    REQUIRE(summary.files[0].status == ssa::ports::ImportFileStatus::Applied);
    REQUIRE(summary.files[0].validRows == 1);
    REQUIRE(summary.files[0].invalidRows == 1);
    REQUIRE(summary.files[0].inserts == 1);
    REQUIRE(summary.files[0].consolidated);
    REQUIRE_FALSE(summary.files[0].noSurvivor);
    REQUIRE(summary.files[1].source == "b-empty.xlsx");
    REQUIRE(summary.files[1].status == ssa::ports::ImportFileStatus::NoValidRows);
    REQUIRE(summary.files[1].validRows == 0);
    REQUIRE(summary.files[1].invalidRows == 0);
    REQUIRE(summary.files[1].noSurvivor);
}

TEST_CASE("rejected workbook summary preserves source and reports no applied rows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "unknown-header.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Unknown A"), inlineCell("B1", "Unknown B")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.importSummary.has_value());
    const auto& summary = *result.importSummary;
    REQUIRE(summary.discovered == 1);
    REQUIRE(summary.accepted == 0);
    REQUIRE(summary.rejected == 1);
    REQUIRE(summary.preserved == 1);
    REQUIRE(summary.validRows == 0);
    REQUIRE(summary.inserts == 0);
    REQUIRE(summary.updates == 0);
    REQUIRE(summary.files.size() == 1);
    REQUIRE(summary.files.front().source == "unknown-header.xlsx");
    REQUIRE(summary.files.front().status == ssa::ports::ImportFileStatus::Rejected);
    REQUIRE_FALSE(summary.files.front().consolidated);
    REQUIRE(std::filesystem::exists(workbook));
}

TEST_CASE("incremental rescan rolls back the batch when a later file is rejected") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto validWorkbook = inputDirectory / "a-valid.xlsx";
    writeWorkbook(
        validWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600602"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Committed first"), inlineCell("D2", "2026-07-01")}));
    const auto invalidWorkbook = inputDirectory / "b-invalid.xlsx";
    writeWorkbook(invalidWorkbook,
                  row(1, {inlineCell("A1", "Unknown A"), inlineCell("B1", "Unknown B")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.importSummary.has_value());
    const auto& summary = *result.importSummary;
    REQUIRE(summary.discovered == 2);
    REQUIRE(summary.accepted == 0);
    REQUIRE(summary.rejected == 2);
    REQUIRE(summary.preserved == 2);
    REQUIRE(summary.inserts == 0);
    REQUIRE(summary.files.size() == 2);
    REQUIRE(summary.files[0].status == ssa::ports::ImportFileStatus::Rejected);
    REQUIRE_FALSE(summary.files[0].consolidated);
    REQUIRE(summary.files[1].status == ssa::ports::ImportFileStatus::Rejected);
    REQUIRE_FALSE(summary.files[1].consolidated);
    REQUIRE(std::filesystem::exists(validWorkbook));
    REQUIRE_FALSE(
        std::filesystem::exists(inputDirectory / "processadas" / validWorkbook.filename()));
    REQUIRE(std::filesystem::exists(invalidWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("incremental cancellation before publication preserves the original database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto processedDirectory = inputDirectory / "processadas";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto firstWorkbook = inputDirectory / "a-first.xlsx";
    writeWorkbook(
        firstWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600603"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Committed before cancel"), inlineCell("D2", "2026-07-01")}));
    const auto secondWorkbook = inputDirectory / "b-second.xlsx";
    writeWorkbook(
        secondWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600604"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Canceled second"), inlineCell("D2", "2026-07-01")}));

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600605"}, {"descricao_ssa", "Original"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    stopSource.request_stop();
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental}, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE_FALSE(std::filesystem::exists(processedDirectory / firstWorkbook.filename()));
    REQUIRE_FALSE(std::filesystem::exists(processedDirectory / secondWorkbook.filename()));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas" / "nosurvivor" /
                                          firstWorkbook.filename()));
    REQUIRE(std::filesystem::exists(firstWorkbook));
    REQUIRE(std::filesystem::exists(secondWorkbook));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600605'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600603'") == 0);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600604'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("incremental cancellation during snapshot publication preserves the original database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    constexpr std::uintmax_t paddingBytes = 128ULL * 1024ULL * 1024ULL;
    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto workbook = inputDirectory / "during-publication.xlsx";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA"),
                                    inlineCell("D1", "Data de emissao")}) +
                                row(2, {inlineCell("A2", "202600606"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Canceled during publication"),
                                        inlineCell("D2", "2026-07-01")}));

    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600607"}, {"descricao_ssa", "Original"}});
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    sqlite3* paddingDb = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &paddingDb) == SQLITE_OK);
    REQUIRE(sqlite3_exec(paddingDb, "CREATE TABLE import_padding(value BLOB)", nullptr, nullptr,
                         nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(paddingDb,
                         "INSERT INTO import_padding(value) VALUES (zeroblob(134217728))", nullptr,
                         nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(paddingDb) == SQLITE_OK);

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    auto future = std::async(std::launch::async, [&] {
        return port.rescan({ssa::ports::RescanMode::Incremental}, stopSource.get_token());
    });
    QElapsedTimer deadline;
    deadline.start();
    bool observedTemporary = false;
    while (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready &&
           deadline.elapsed() < 5'000) {
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(dbPath.parent_path(), error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->path().filename() == dbPath.filename() ||
                !iterator->is_regular_file(error) || iterator->file_size(error) == 0 || error) {
                continue;
            }
            observedTemporary = true;
            stopSource.request_stop();
            break;
        }
        QThread::msleep(1);
    }
    stopSource.request_stop();
    const auto result = future.get();

    REQUIRE(observedTemporary);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas" / workbook.filename()));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600607'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600606'") == 0);
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

TEST_CASE("spreadsheet workflow rejects ambiguous positional headers without moving the source") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "ambiguous.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Número da SSA Relacionada"),
                inlineCell("C1", "Numero SSA"), inlineCell("D1", "Descricao"),
                inlineCell("E1", "Data Cadastro")}) +
            row(2, {inlineCell("A2", "202600001"), inlineCell("B2", "202600002"),
                    inlineCell("C2", "202600003"), inlineCell("D2", "Colisao"),
                    inlineCell("E2", "2026-07-14")}));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("ambiguous_headers") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
}

TEST_CASE("spreadsheet workflow skips a cover sheet and imports the following data sheet") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "two-sheets.xlsx";
    const auto header = row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                                inlineCell("C1", "Data Cadastro")});
    const auto firstData =
        row(2, {inlineCell("A2", "202600321"), inlineCell("B2", "Imported from sheet two"),
                inlineCell("C2", "2026-07-14")});
    const auto secondData =
        row(2, {inlineCell("A2", "202600322"), inlineCell("B2", "Imported from sheet three"),
                inlineCell("C2", "2026-07-14")});
    writeWorkbookSheets(workbook, {row(1, {inlineCell("A1", "SAM report cover")}),
                                   header + firstData, header + secondData});
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(result.importSummary.has_value());
    REQUIRE(result.importSummary->validRows == 2);
    REQUIRE(result.importSummary->inserts == 2);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 2);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600321'") ==
            "Imported from sheet two");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet workflow rejects an unknown sheet after valid data") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "invalid-tail.xlsx";
    const auto valid = row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                               inlineCell("C1", "Data Cadastro")}) +
                       row(2, {inlineCell("A2", "202600323"), inlineCell("B2", "Must roll back"),
                               inlineCell("C2", "2026-07-14")});
    writeWorkbookSheets(workbook,
                        {valid, row(1, {inlineCell("A1", "Unexpected trailing report")})});
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600100"}, {"descricao_ssa", "Existing"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(std::filesystem::exists(workbook));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600100'") ==
            "Existing");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet workflow rolls back conflicting SSA rows across worksheets") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = inputDirectory / "conflict.xlsx";
    const auto header = row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Descricao"),
                                inlineCell("C1", "Data Cadastro")});
    writeWorkbookSheets(workbook,
                        {header + row(2, {inlineCell("A2", "202600324"), inlineCell("B2", "First"),
                                          inlineCell("C2", "2026-07-14")}),
                         header + row(2, {inlineCell("A2", "202600324"), inlineCell("B2", "Second"),
                                          inlineCell("C2", "2026-07-14")})});
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600100"}, {"descricao_ssa", "Existing"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("duplicate_conflict") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("spreadsheet mapper observes cancellation during a large worksheet") {
    ssa::infra::importing::SpreadsheetTable table;
    table.rows.reserve(250'001);
    table.rows.push_back({"Numero SSA", "Descricao", "Data Cadastro"});
    for (std::size_t index = 0; index < 250'000; ++index) {
        table.rows.push_back({"202600321", "Large worksheet", "2026-07-14"});
    }
    std::stop_source stopSource;
    std::latch started{1};
    auto operation = std::async(std::launch::async, [&] {
        started.count_down();
        try {
            static_cast<void>(
                ssa::infra::importing::SsaSpreadsheetMapper::map(table, stopSource.get_token()));
            return false;
        } catch (const std::system_error& error) {
            return error.code() == std::make_error_code(std::errc::operation_canceled);
        }
    });
    started.wait();
    QThread::msleep(5);

    stopSource.request_stop();

    REQUIRE(operation.wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    REQUIRE(operation.get());
}

TEST_CASE("full rescan rejects mixed valid and invalid rows without clearing the database") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back({{"numero_ssa", "202600519"}, {"descricao_ssa", "Previous"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    const auto workbook = inputDirectory / "mixed.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600520"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Valid"), inlineCell("D2", "2026-07-01")}) +
            row(3, {inlineCell("A3", "invalid"), inlineCell("B3", "APV"),
                    inlineCell("C3", "Invalid"), inlineCell("D3", "2026-07-01")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("invalid_rows=1") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600519'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600520'") == 0);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
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
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    const auto validWorkbook = inputDirectory / "a-valid.xlsx";
    writeWorkbook(
        validWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600405"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Nova"), inlineCell("D2", "2026-07-01")}));
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
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);
    const auto workbook = inputDirectory / "header-only.xlsx";
    writeWorkbook(workbook, row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                                    inlineCell("C1", "Descricao da SSA"),
                                    inlineCell("D1", "Data de emissao")}));

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

TEST_CASE("full rescan rolls back valid data when another workbook has no valid rows") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    ssa::infra::importing::ResolvedSsaImportRows previous;
    previous.rows.push_back(
        {{"numero_ssa", "202600412"}, {"situacao", "ASE"}, {"descricao_ssa", "Anterior"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    REQUIRE(writer.write(previous, 1, 0, false).rowsWritten == 1);

    const auto validWorkbook = inputDirectory / "a-valid.xlsx";
    writeWorkbook(
        validWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600413"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Nova"), inlineCell("D2", "2026-07-01")}));
    const auto emptyWorkbook = inputDirectory / "b-empty.xlsx";
    writeWorkbook(
        emptyWorkbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("no_valid_rows") != std::string::npos);
    REQUIRE(std::filesystem::exists(validWorkbook));
    REQUIRE(std::filesystem::exists(emptyWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600412'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600413'") == 0);
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
                                    inlineCell("C1", "Descricao da SSA"),
                                    inlineCell("D1", "Data de emissao")}) +
                                row(2, {inlineCell("A2", "202600406"), inlineCell("B2", "ASE"),
                                        inlineCell("C2", "Nova"), inlineCell("D2", "2026-07-01")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Full});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("rescan database snapshot failed") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE(std::filesystem::exists(workbook));
}

TEST_CASE("spreadsheet import workflow leaves selected legacy xls pending") {
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
    REQUIRE(result.message.find("converted_xls=0") != std::string::npos);
    REQUIRE(result.diagnostic.empty());
    REQUIRE(std::filesystem::exists(legacy));
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
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                inlineCell("C1", "Descricao da SSA"), inlineCell("D1", "Data de emissao")}) +
            row(2, {inlineCell("A2", "202600128"), inlineCell("B2", "ASE"),
                    inlineCell("C2", "Existing workbook"), inlineCell("D2", "2026-07-01")}));
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

TEST_CASE("import file stager rejects an input path that is not a directory") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto inputPath = std::filesystem::path{tempDir.path().toStdString()} / "not-a-folder";
    {
        std::ofstream output(inputPath);
        output << "not a directory";
    }

    const ssa::infra::importing::ImportFileStager stager(inputPath);

    const auto result = stager.stageInputFiles();

    REQUIRE(result.rejectionReason == "input_directory_not_directory");
    REQUIRE(result.files.empty());
}

#ifndef _WIN32
TEST_CASE("spreadsheet import workflow reports an unreadable input directory as failed") {
    if (::geteuid() == 0) {
        SKIP("permission scan failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::permissions(inputDirectory, std::filesystem::perms::none,
                                 std::filesystem::perm_options::replace);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    std::filesystem::permissions(inputDirectory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("input_directory_unreadable") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}

TEST_CASE("spreadsheet import workflow reports input status IO errors as failed") {
    if (::geteuid() == 0) {
        SKIP("permission status failure cannot be simulated as root");
    }
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto blockedDirectory = root / "blocked";
    const auto inputDirectory = blockedDirectory / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(blockedDirectory);
    std::filesystem::permissions(blockedDirectory, std::filesystem::perms::none,
                                 std::filesystem::perm_options::replace);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    std::filesystem::permissions(blockedDirectory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("input_directory_status_unavailable") != std::string::npos);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
}
#endif

#ifndef _WIN32
TEST_CASE("spreadsheet import workflow does not invoke the legacy converter") {
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

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    ssa::ports::ImportExternalFilesRequest request;
    request.files = {legacyWorkbook};

    const auto result = port.importExternalFiles(request);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("legacy_xls=1") != std::string::npos);
    REQUIRE(result.message.find("converted_xls=0") != std::string::npos);
    REQUIRE_FALSE(std::filesystem::exists(dbPath));
    REQUIRE(std::filesystem::exists(legacyWorkbook));
}

TEST_CASE("SAM manifest count mismatch rolls back the entire external import") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = sourceDirectory / "result.xlsx";
    writeWorkbook(workbook,
                  row(1, {inlineCell("A1", "ssa_number"), inlineCell("B1", "localization"),
                          inlineCell("C1", "description"), inlineCell("D1", "issue_datetime"),
                          inlineCell("E1", "emission_datetime"), inlineCell("F1", "emitter_sector"),
                          inlineCell("G1", "executor_sector"), inlineCell("H1", "year_week"),
                          inlineCell("I1", "situation_desc"), inlineCell("J1", "process_status"),
                          inlineCell("K1", "detail_present")}) +
                      row(2, {inlineCell("A2", "202600701"), inlineCell("B2", "LOC-1"),
                              inlineCell("C2", "SAM row"), inlineCell("D2", "2026-07-15T08:00:00Z"),
                              inlineCell("E2", "2026-07-15T09:00:00Z"), inlineCell("F2", "IEE2"),
                              inlineCell("G2", "IEE3"), inlineCell("H2", "202629"),
                              inlineCell("I2", "STE - SERVICO TERMINADO"),
                              inlineCell("J2", "SSA Terminada"), inlineCell("K2", "true")}));
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600700"},
                             {"descricao_ssa", "Existing row"},
                             {"data_cadastro", "2026-07-14"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importSamArtifacts({{{workbook, "IEE3", 2, 1, 1}}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("physical_count_mismatch") != std::string::npos);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600700'") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600701'") == 0);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    REQUIRE(std::filesystem::exists(workbook));
}

TEST_CASE("SAM service validates and imports a real workbook into SQLite") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = sourceDirectory / "result.xlsx";
    writeWorkbook(
        workbook,
        row(1, {inlineCell("A1", "ssa_number"), inlineCell("B1", "localization"),
                inlineCell("C1", "description"), inlineCell("D1", "issue_datetime"),
                inlineCell("E1", "emission_datetime"), inlineCell("F1", "emitter_sector"),
                inlineCell("G1", "executor_sector"), inlineCell("H1", "year_week"),
                inlineCell("I1", "situation_desc"), inlineCell("J1", "process_status"),
                inlineCell("K1", "detail_present")}) +
            row(2, {inlineCell("A2", "202600702"), inlineCell("B2", "LOC-2"),
                    inlineCell("C2", "Imported from SAM"), inlineCell("D2", "2026-01-01T06:00:00Z"),
                    inlineCell("E2", "23/02/2026 10:52:02"), inlineCell("F2", "MEL3"),
                    inlineCell("G2", "MEL4"), inlineCell("H2", "202608"),
                    inlineCell("I2", "AGUARDANDO PROGRAMACAO"), inlineCell("J2", "SSA Planejada"),
                    inlineCell("K2", "true")}));
    auto importPort = std::make_shared<ssa::infra::importing::SpreadsheetImportWorkflowPort>(
        inputDirectory, dbPath, importColumns());
    auto samPort = std::make_shared<StaticSamPort>();
    samPort->result = {
        ssa::ports::WorkflowStatus::Succeeded, "fetched", {{workbook, "MEL4", 1, 1, 0}}};
    const ssa::application::SsaWorkflowService service(importPort, nullptr, nullptr, nullptr,
                                                       samPort, importPort);

    const auto result = service.refreshSam({});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(samPort->discardCalls == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600702'") == 1);
    REQUIRE(scalarText(db, "SELECT descricao_ssa FROM ssa_table WHERE numero_ssa='202600702'") ==
            "Imported from SAM");
    REQUIRE(scalarText(db, "SELECT setor_executor FROM ssa_table WHERE numero_ssa='202600702'") ==
            "MEL4");
    REQUIRE(scalarText(db, "SELECT situacao FROM ssa_table WHERE numero_ssa='202600702'") == "APG");
    REQUIRE(scalarText(db, "SELECT data_cadastro FROM ssa_table WHERE numero_ssa='202600702'") ==
            "2026-02-23 10:52:02");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("SAM physical row mismatch rolls back valid rows hidden by an invalid row") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto workbook = sourceDirectory / "result.xlsx";
    const auto headers =
        row(1, {inlineCell("A1", "ssa_number"), inlineCell("B1", "localization"),
                inlineCell("C1", "description"), inlineCell("D1", "issue_datetime"),
                inlineCell("E1", "emission_datetime"), inlineCell("F1", "emitter_sector"),
                inlineCell("G1", "executor_sector"), inlineCell("H1", "year_week"),
                inlineCell("I1", "situation_desc"), inlineCell("J1", "process_status"),
                inlineCell("K1", "detail_present")});
    const auto validRow =
        row(2, {inlineCell("A2", "202600703"), inlineCell("B2", "LOC-3"),
                inlineCell("C2", "Valid SAM row"), inlineCell("D2", "2026-07-15T08:00:00Z"),
                inlineCell("E2", "2026-07-15T09:00:00Z"), inlineCell("F2", "IEE2"),
                inlineCell("G2", "IEE3"), inlineCell("H2", "202629"),
                inlineCell("I2", "STE - SERVICO TERMINADO"), inlineCell("J2", "SSA Terminada"),
                inlineCell("K2", "true")});
    const auto invalidRow =
        row(3, {inlineCell("A3", "invalid"), inlineCell("B3", "LOC-4"),
                inlineCell("C3", "Invalid SAM row"), inlineCell("D3", "2026-07-15T08:00:00Z"),
                inlineCell("E3", "2026-07-15T09:00:00Z"), inlineCell("F3", "IEE2"),
                inlineCell("G3", "IEE3"), inlineCell("H3", "202629"),
                inlineCell("I3", "STE - SERVICO TERMINADO"), inlineCell("J3", "SSA Terminada"),
                inlineCell("K3", "false")});
    writeWorkbook(workbook, headers + validRow + invalidRow);
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600700"},
                             {"descricao_ssa", "Existing row"},
                             {"data_cadastro", "2026-07-14"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importSamArtifacts({{{workbook, "IEE3", 1, 1, 0}}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("physical_count_mismatch") != std::string::npos);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600700'") == 1);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    REQUIRE(std::filesystem::exists(workbook));
}

TEST_CASE("SAM import stages every sector before opening the atomic write session") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto validWorkbook = sourceDirectory / "valid.xlsx";
    const auto missingWorkbook = sourceDirectory / "missing.xlsx";
    writeWorkbook(validWorkbook, samWorkbookRows("202600704", "IEE3", "First sector"));
    ssa::infra::importing::ResolvedSsaImportRows seedRows;
    seedRows.rows.push_back({{"numero_ssa", "202600700"},
                             {"descricao_ssa", "Existing row"},
                             {"data_cadastro", "2026-07-14"}});
    const ssa::infra::sqlite::SqliteSsaImportWriter seedWriter(dbPath, importColumns());
    REQUIRE(seedWriter.write(seedRows, 1, 0, false).rowsWritten == 1);
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importSamArtifacts(
        {{{validWorkbook, "IEE3", 1, 1, 0}, {missingWorkbook, "MEL4", 1, 1, 0}}});

    REQUIRE_FALSE(result.ok());
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table") == 1);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600704'") == 0);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    REQUIRE(std::filesystem::exists(validWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
}

TEST_CASE("SAM import resumes prior consolidation and still imports the current batch") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto pendingSource = inputDirectory / "pending.xlsx";
    const auto pendingDestination = inputDirectory / "processadas" / "pending.xlsx";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    createSparseFile(pendingSource, 1);
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation({{{pendingSource, pendingDestination, true}}});
    static_cast<void>(session.finish());
    const auto workbook = sourceDirectory / "current.xlsx";
    writeWorkbook(workbook, samWorkbookRows("202600705", "IEE3", "Current SAM batch"));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importSamArtifacts({{{workbook, "IEE3", 1, 1, 0}}});

    INFO(result.message);
    INFO(result.diagnostic);
    REQUIRE(result.ok());
    REQUIRE_FALSE(std::filesystem::exists(pendingSource));
    REQUIRE(std::filesystem::exists(pendingDestination));
    REQUIRE(writer.pendingConsolidation().empty());
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600705'") == 1);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("SAM import reports cancellation while resuming a prior journal") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto pendingSource = inputDirectory / "pending.xlsx";
    const auto pendingDestination = inputDirectory / "processadas" / "pending.xlsx";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    createSparseFile(pendingSource, 1);
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation({{{pendingSource, pendingDestination, true}}});
    static_cast<void>(session.finish());
    const auto workbook = sourceDirectory / "current.xlsx";
    writeWorkbook(workbook, samWorkbookRows("202600706", "IEE3", "Canceled SAM batch"));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result =
        port.importSamArtifacts({{{workbook, "IEE3", 1, 1, 0}}}, stopSource.get_token());

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled_during_consolidation_resume") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE(std::filesystem::exists(pendingSource));
    REQUIRE_FALSE(std::filesystem::exists(pendingDestination));
    REQUIRE(writer.pendingConsolidation().size() == 1);
}

TEST_CASE("SAM import preserves a real failure while resuming a prior journal") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto sourceDirectory = root / "sam";
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    const auto missingSource = inputDirectory / "missing-pending.xlsx";
    const auto missingDestination = inputDirectory / "processadas" / "missing-pending.xlsx";
    std::filesystem::create_directories(sourceDirectory);
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const ssa::infra::sqlite::SqliteSsaImportWriter writer(dbPath, importColumns());
    auto session = writer.startSession(false);
    session.recordConsolidation({{{missingSource, missingDestination, true}}});
    static_cast<void>(session.finish());
    const auto workbook = sourceDirectory / "current.xlsx";
    writeWorkbook(workbook, samWorkbookRows("202600707", "IEE3", "Blocked SAM batch"));
    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());

    const auto result = port.importSamArtifacts({{{workbook, "IEE3", 1, 1, 0}}});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Failed);
    REQUIRE(result.message.find("resume_failed") != std::string::npos);
    REQUIRE(std::filesystem::exists(workbook));
    REQUIRE(writer.pendingConsolidation().size() == 1);
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(dbPath.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(scalarInt(db, "SELECT COUNT(*) FROM ssa_table WHERE numero_ssa='202600707'") == 0);
    REQUIRE(scalarText(db, "PRAGMA integrity_check") == "ok");
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

TEST_CASE("incremental rescan leaves legacy xls pending without conversion") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto root = std::filesystem::path{tempDir.path().toStdString()};
    const auto inputDirectory = root / "docs_entrada";
    const auto dbPath = root / "data" / "ssas.db";
    std::filesystem::create_directories(inputDirectory);
    std::filesystem::create_directories(dbPath.parent_path());
    const auto legacyWorkbook = inputDirectory / "legacy-input.xls";
    writeWorkbook(legacyWorkbook,
                  row(1, {inlineCell("A1", "Numero SSA"), inlineCell("B1", "Situacao"),
                          inlineCell("C1", "Descricao da SSA")}) +
                      row(2, {inlineCell("A2", "202600123"), inlineCell("B2", "ASE"),
                              inlineCell("C2", "Legado da entrada")}));

    ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, dbPath,
                                                              importColumns());
    const auto result = port.rescan({ssa::ports::RescanMode::Incremental});

    INFO(result.message);
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Rejected);
    REQUIRE(result.message.find("legacy_xls=1") != std::string::npos);
    REQUIRE(std::filesystem::exists(legacyWorkbook));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "legacy-input.xlsx"));
    REQUIRE_FALSE(std::filesystem::exists(inputDirectory / "processadas"));
}
#endif
