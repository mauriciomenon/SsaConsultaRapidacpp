#include "domain/ColumnCatalog.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "ports/IWorkflowPorts.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>

#include <miniz.h>
#include <sqlite3.h>

#include <sys/resource.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

    constexpr std::size_t kFixtureRows = 250'000;
    constexpr std::uint64_t kMaxAdditionalRssBytes = 256ULL * 1024ULL * 1024ULL;

    bool addEntry(mz_zip_archive& zip, const char* path, const std::string& content) {
        return mz_zip_writer_add_mem(&zip, path, content.data(), content.size(),
                                     MZ_BEST_COMPRESSION) != 0;
    }

    bool writeWorksheetXml(const std::filesystem::path& path) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        output
            << R"(<?xml version="1.0" encoding="UTF-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<dimension ref="A1:C250001"/><sheetData>)"
            << R"(<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c><c r="C1" t="s"><v>2</v></c></row>)";
        for (std::size_t index = 0; index < kFixtureRows; ++index) {
            const auto rowNumber = index + 2;
            const auto ssaNumber = 700'000'000 + index;
            output << "<row r=\"" << rowNumber << "\"><c r=\"A" << rowNumber << "\"><v>"
                   << ssaNumber << "</v></c><c r=\"B" << rowNumber
                   << "\" t=\"s\"><v>3</v></c><c r=\"C" << rowNumber
                   << "\" t=\"s\"><v>4</v></c></row>";
        }
        output << "</sheetData></worksheet>";
        return output.good();
    }

    bool writeFixture(const std::filesystem::path& inputDirectory) {
        std::filesystem::create_directories(inputDirectory);
        const auto worksheet = inputDirectory.parent_path() / "sheet1.xml";
        if (!writeWorksheetXml(worksheet)) {
            return false;
        }
        const auto workbook = inputDirectory / "memory-probe.xlsx";
        mz_zip_archive zip{};
        const bool opened = mz_zip_writer_init_file(&zip, workbook.string().c_str(), 0) != 0;
        const bool metadataAdded =
            opened && addEntry(zip, "[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
</Types>)") &&
            addEntry(zip, "xl/workbook.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets><sheet name="Data" sheetId="1" r:id="rId1"/></sheets></workbook>)") &&
            addEntry(zip, "xl/_rels/workbook.xml.rels",
                     R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
</Relationships>)") &&
            addEntry(zip, "xl/sharedStrings.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="5" uniqueCount="5">
<si><t>Numero SSA</t></si><si><t>Descricao</t></si><si><t>Data Cadastro</t></si>
<si><t>Memory probe</t></si><si><t>2026-07-14</t></si></sst>)") &&
            mz_zip_writer_add_file(&zip, "xl/worksheets/sheet1.xml", worksheet.string().c_str(),
                                   nullptr, 0, MZ_BEST_COMPRESSION) != 0;
        const bool finalized = metadataAdded && mz_zip_writer_finalize_archive(&zip) != 0;
        const bool closed = !opened || mz_zip_writer_end(&zip) != 0;
        std::error_code removeError;
        std::filesystem::remove(worksheet, removeError);
        return finalized && closed && !removeError;
    }

    std::uint64_t currentRssBytes() {
#ifdef __APPLE__
        mach_task_basic_info_data_t info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                      &count) != KERN_SUCCESS) {
            return 0;
        }
        return info.resident_size;
#else
        std::ifstream status("/proc/self/statm");
        std::uint64_t totalPages = 0;
        std::uint64_t residentPages = 0;
        if (!(status >> totalPages >> residentPages)) {
            return 0;
        }
        return residentPages * static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
#endif
    }

    std::uint64_t peakRssBytes() {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0;
        }
#ifdef __APPLE__
        return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
    }

    std::size_t importedRows(const std::filesystem::path& databasePath) {
        sqlite3* database = nullptr;
        if (sqlite3_open_v2(databasePath.string().c_str(), &database, SQLITE_OPEN_READONLY,
                            nullptr) != SQLITE_OK) {
            if (database != nullptr) {
                sqlite3_close(database);
            }
            return 0;
        }
        sqlite3_stmt* statement = nullptr;
        std::size_t count = 0;
        if (sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM ssa_table", -1, &statement,
                               nullptr) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_ROW) {
            count = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
        }
        if (statement != nullptr) {
            sqlite3_finalize(statement);
        }
        sqlite3_close(database);
        return count;
    }

    int runWorker(const std::filesystem::path& root) {
        const auto inputDirectory = root / "docs_entrada";
        const auto databasePath = root / "data" / "ssas.db";
        std::filesystem::create_directories(databasePath.parent_path());
        const auto baseline = currentRssBytes();
        const auto columns = ssa::domain::ColumnCatalog::all();
        ssa::infra::importing::SpreadsheetImportWorkflowPort port(inputDirectory, databasePath,
                                                                  {columns.begin(), columns.end()});
        const auto result = port.rescan({ssa::ports::RescanMode::Incremental});
        const auto peak = peakRssBytes();
        if (baseline == 0 || peak <= baseline) {
            std::cerr << "SSA_IMPORT_RSS measurement unavailable baseline=" << baseline
                      << " peak=" << peak << '\n';
            return 5;
        }
        const auto additional = peak - baseline;
        const auto rows = importedRows(databasePath);
        std::cout << "SSA_IMPORT_RSS rows=" << rows << " baseline=" << baseline << " peak=" << peak
                  << " additional=" << additional << '\n';
        if (!result.ok()) {
            std::cerr << result.message << '\n' << result.diagnostic << '\n';
            return 2;
        }
        if (rows != kFixtureRows) {
            return 3;
        }
        return additional <= kMaxAdditionalRssBytes ? 0 : 4;
    }

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--worker")) {
        return runWorker(std::filesystem::path{arguments.at(2).toStdString()});
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return 2;
    }
    const auto root = std::filesystem::path{temporary.path().toStdString()};
    if (!writeFixture(root / "docs_entrada")) {
        return 3;
    }
    QProcess worker;
    worker.setProgram(QCoreApplication::applicationFilePath());
    worker.setArguments({QStringLiteral("--worker"), temporary.path()});
    worker.setProcessChannelMode(QProcess::MergedChannels);
    worker.start();
    if (!worker.waitForStarted(10'000) || !worker.waitForFinished(180'000)) {
        worker.kill();
        worker.waitForFinished();
        return 4;
    }
    std::cout << worker.readAll().toStdString();
    return worker.exitStatus() == QProcess::NormalExit ? worker.exitCode() : 5;
}
