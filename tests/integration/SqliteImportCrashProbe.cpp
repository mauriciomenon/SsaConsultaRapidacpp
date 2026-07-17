#include "SqliteSsaImportWriterTestAccess.h"
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
    if (argc != 4 && argc != 6) {
        std::cerr << "usage: sqlite-import-crash-probe <db> <ready> <before|after|journal-before-"
                     "move|journal-after-move|journal-delete-before-commit|journal-delete-after-"
                     "commit> [source destination]\n";
        return 2;
    }

    const auto databasePath = std::filesystem::path{argv[1]};
    const auto readyPath = std::filesystem::path{argv[2]};
    const auto scenario = std::string_view{argv[3]};
    const bool journalMoveScenario =
        scenario == "journal-before-move" || scenario == "journal-after-move";
    const bool journalDeleteScenario =
        scenario == "journal-delete-before-commit" || scenario == "journal-delete-after-commit";
    const bool journalScenario = journalMoveScenario || journalDeleteScenario;
    const bool commitBeforeReady = scenario == "after" || journalScenario;
    if (journalScenario != (argc == 6)) {
        std::cerr << "journal scenarios require source and destination\n";
        return 2;
    }
    if (readyPath.parent_path() != databasePath.parent_path()) {
        std::cerr << "ready marker must share the database directory\n";
        return 2;
    }
    try {
        const std::vector<ssa::domain::ColumnDef> columns{
            {.key = "numero_ssa", .label = "Numero", .labelFull = "Numero"},
            {.key = "descricao_ssa", .label = "Descricao", .labelFull = "Descricao"}};
        const ssa::infra::sqlite::SqliteSsaImportWriter writer(
            ssa::infra::sqlite::SqliteSsaImportWriterTestAccess::access(), databasePath, columns);
        if (journalDeleteScenario) {
            writer.completeConsolidation(writer.pendingConsolidation());
        } else {
            ssa::infra::importing::ResolvedSsaImportRows replacement;
            replacement.rows.push_back({{"numero_ssa", "202600211"}, {"descricao_ssa", "Nova"}});

            auto session = writer.startSession(true);
            if (session.write(replacement, 1, 0).conflictRows != 0) {
                std::cerr << "unexpected import conflict\n";
                return 3;
            }
            if (journalScenario) {
                session.recordConsolidation(
                    {{{std::filesystem::path{argv[4]}}, {std::filesystem::path{argv[5]}}, true}});
            }
            if (commitBeforeReady) {
                static_cast<void>(session.finish());
            }
            if (scenario == "journal-after-move") {
                const auto source = std::filesystem::path{argv[4]};
                const auto destination = std::filesystem::path{argv[5]};
                std::filesystem::create_directories(destination.parent_path());
                std::filesystem::rename(source, destination);
            }
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
