// Diagnostic harness: drives BrowseViewModel with a real SqliteSsaRepository
// against data/ssas.db and samples RSS across many page navigations.
// Not part of shipped binaries; built only when invoked explicitly.

#include "infra/sqlite/SqliteSsaRepository.h"
#include "presentation/BrowseViewModel.h"
#include "query/SsaQueryService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QString>
#include <QTimer>

#include <cstdio>
#include <filesystem>
#include <memory>

#ifdef __APPLE__
#include <mach/mach.h>
static std::size_t currentRssKb() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) !=
        KERN_SUCCESS) {
        return 0;
    }
    return static_cast<std::size_t>(info.phys_footprint) / 1024;
}
#else
#include <sys/resource.h>
static std::size_t currentRssKb() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __linux__
    return static_cast<std::size_t>(usage.ru_maxrss);
#else
    return static_cast<std::size_t>(usage.ru_maxrss) / 1024;
#endif
}
#endif

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    QString dbPath;
    int totalPages = 200;
    int pageSize = 50;
    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a == "--db" && i + 1 < args.size()) {
            dbPath = args[++i];
        } else if (a == "--pages" && i + 1 < args.size()) {
            totalPages = args[++i].toInt();
        } else if (a == "--page-size" && i + 1 < args.size()) {
            pageSize = args[++i].toInt();
        }
    }
    if (dbPath.isEmpty()) {
        std::fprintf(stderr, "usage: ssa_mem_stress --db <path> [--pages N] [--page-size N]\n");
        return 1;
    }

    auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(
        std::filesystem::path{dbPath.toStdString()});
    auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
    ssa::presentation::BrowseViewModel browse(service);

    browse.setPageSize(pageSize);
    browse.load();

    auto waitPage = [&]() {
        QEventLoop loop;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        QObject::connect(&browse, &ssa::presentation::BrowseViewModel::pageChanged, &loop,
                         &QEventLoop::quit);
        loop.exec();
    };
    waitPage();

    std::printf("phase\tpages\trss_kb\n");
    std::printf("startup\t0\t%zu\n", currentRssKb());
    std::printf("firstpage\t1\t%zu\n", currentRssKb());

    for (int i = 2; i <= totalPages; ++i) {
        browse.nextPage();
        waitPage();
        if (i % 10 == 0) {
            std::printf("nav\t%d\t%zu\n", i, currentRssKb());
            std::fflush(stdout);
        }
        if (browse.pageNumber() >= browse.pageCount()) {
            browse.clearSearchAndResetPage();
            waitPage();
        }
    }
    std::printf("final\t%d\t%zu\n", totalPages, currentRssKb());

    return 0;
}
