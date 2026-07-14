#include "infra/SsaImportData.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "qt/FilesystemPath.h"

#include <QIODevice>
#include <QSaveFile>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

int main(const int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: sqlite-import-crash-probe <db> <ready> <before|after>\n";
        return 2;
    }

    const auto databasePath = std::filesystem::path{argv[1]};
    const auto readyPath = std::filesystem::path{argv[2]};
    const auto commitBeforeReady = std::string_view{argv[3]} == "after";
    if (readyPath.parent_path() != databasePath.parent_path()) {
        std::cerr << "ready marker must share the database directory\n";
        return 2;
    }
    try {
        const std::vector<ssa::domain::ColumnDef> columns{
            {.key = "numero_ssa", .label = "Numero", .labelFull = "Numero"},
            {.key = "descricao_ssa", .label = "Descricao", .labelFull = "Descricao"}};
        const ssa::infra::sqlite::SqliteSsaImportWriter writer(databasePath, columns);
        ssa::infra::importing::ResolvedSsaImportRows replacement;
        replacement.rows.push_back({{"numero_ssa", "202600211"}, {"descricao_ssa", "Nova"}});

        auto session = writer.startSession(true);
        if (session.write(replacement, 1, 0).conflictRows != 0) {
            std::cerr << "unexpected import conflict\n";
            return 3;
        }
        if (commitBeforeReady) {
            static_cast<void>(session.finish());
        }

        QSaveFile ready(ssa::qt::toQString(readyPath));
        if (!ready.open(QIODevice::WriteOnly) || ready.write("ready\n") != 6 || !ready.commit()) {
            std::cerr << "failed to publish ready marker\n";
            return 3;
        }
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 4;
    }
}
