// Diagnostic harness: drives BrowseViewModel with a real SqliteSsaRepository
// against a database and samples RSS across many page navigations.
// Built only on non-Windows (uses POSIX getrusage / mach task_info).

#include "infra/sqlite/SqliteSsaRepository.h"
#include "presentation/BrowseViewModel.h"
#include "query/SsaQueryService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

#ifdef __APPLE__
#include <mach/mach.h>
static std::size_t currentFootprintKb() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) !=
        KERN_SUCCESS) {
        return 0;
    }
    return static_cast<std::size_t>(info.phys_footprint) / 1024;
}
#elif defined(__linux__)
#include <cstdio>
static std::size_t currentFootprintKb() {
    // /proc/self/statm reports current resident pages, unlike getrusage.ru_maxrss
    // which is peak RSS since process start and plateaus early.
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (f == nullptr) {
        return 0;
    }
    unsigned long pages = 0;
    int matched = std::fscanf(f, "%lu", &pages);
    std::fclose(f);
    if (matched != 1) {
        return 0;
    }
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        pageSize = 4096;
    }
    return (pages * static_cast<std::size_t>(pageSize)) / 1024;
}
#else
#include <sys/resource.h>
static std::size_t currentFootprintKb() {
    // Fallback: peak RSS (KB on BSD/macOS, bytes on Linux - but Linux branch above
    // handles /proc). On other BSDs ru_maxrss is in KB.
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return static_cast<std::size_t>(usage.ru_maxrss);
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

    std::filesystem::path dbFilePath{dbPath.toStdString()};
    std::error_code ec;
    if (!std::filesystem::exists(dbFilePath, ec)) {
        std::fprintf(stderr, "error: database file does not exist: %s\n",
                     dbFilePath.string().c_str());
        return 1;
    }

    auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(dbFilePath);
    auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
    ssa::presentation::BrowseViewModel browse(service);

    // Persistent connection tracks pageChanged emissions across waitPage calls
    // so a signal fired before waitPage connects is not lost.
    bool pageChangedEmitted = false;
    QObject::connect(&browse, &ssa::presentation::BrowseViewModel::pageChanged,
                     [&]() { pageChangedEmitted = true; });

    // Returns true if pageChanged fired within the timeout, false on timeout.
    // The persistent connection above is checked first to catch signals emitted
    // before this call. We also process pending events before the check to
    // close the race window where a signal is queued but not yet delivered.
    auto waitPage = [&]() -> bool {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (pageChangedEmitted) {
            pageChangedEmitted = false;
            return true;
        }
        QEventLoop loop;
        bool signalReceived = false;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(5000);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        auto conn = QObject::connect(&browse, &ssa::presentation::BrowseViewModel::pageChanged,
                                     &loop, [&]() {
                                         signalReceived = true;
                                         pageChangedEmitted = false;
                                         loop.quit();
                                     });
        timeoutTimer.start();
        loop.exec();
        QObject::disconnect(conn);
        return signalReceived;
    };

    browse.setPageSize(pageSize);
    browse.load();
    if (!waitPage()) {
        std::fprintf(stderr, "error: initial page load timed out\n");
        return 1;
    }

    // macOS: phys_footprint (real app-owned memory). Linux: current resident
    // pages via /proc/self/statm. Other BSDs: peak RSS fallback (noted in
    // currentFootprintKb). Label reflects the common semantic, not raw RSS.
    std::printf("phase\tpages\tfootprint_kb\n");
    std::printf("startup\t0\t%zu\n", currentFootprintKb());
    std::printf("firstpage\t1\t%zu\n", currentFootprintKb());

    int lastPageNumber = browse.pageNumber();
    for (int i = 2; i <= totalPages; ++i) {
        const int pageCount = browse.pageCount();
        const int pageNumber = browse.pageNumber();
        // Guard against single-page or empty result sets: navigating next would
        // be a no-op or wrap, and the reset branch would loop forever.
        if (pageCount <= 1 || pageNumber >= pageCount) {
            browse.clearSearchAndResetPage();
            if (!waitPage()) {
                std::fprintf(stderr, "error: reset timed out at iteration %d\n", i);
                return 1;
            }
        } else {
            browse.nextPage();
            if (!waitPage()) {
                std::fprintf(stderr, "error: nextPage timed out at iteration %d\n", i);
                return 1;
            }
        }
        // Guard against the model failing to advance pages, which would make
        // the loop spin without progress and waste resources.
        const int currentPage = browse.pageNumber();
        if (currentPage == lastPageNumber && pageCount > 1) {
            std::fprintf(stderr, "error: page did not advance at iteration %d (stuck at %d)\n", i,
                         currentPage);
            return 1;
        }
        lastPageNumber = currentPage;
        if (i % 10 == 0) {
            std::printf("nav\t%d\t%zu\n", i, currentFootprintKb());
            std::fflush(stdout);
        }
    }
    std::printf("final\t%d\t%zu\n", totalPages, currentFootprintKb());

    return 0;
}
