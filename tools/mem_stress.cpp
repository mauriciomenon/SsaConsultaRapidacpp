// Diagnostic harness: drives BrowseViewModel with a real SqliteSsaRepository
// against a database and samples RSS across many page navigations.
// Built only on non-Windows (uses POSIX getrusage / mach task_info).

#include "infra/sqlite/SqliteSsaRepository.h"
#include "presentation/BrowseViewModel.h"
#include "qt/FilesystemPath.h"
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
#include <unistd.h>
static std::size_t currentFootprintKb() {
    // /proc/self/statm fields: size resident shared text lib data dt (in pages).
    // Field 2 (resident) is current resident pages, unlike getrusage.ru_maxrss
    // which is peak RSS since process start and plateaus early.
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (f == nullptr) {
        return 0;
    }
    unsigned long total = 0;
    unsigned long resident = 0;
    int matched = std::fscanf(f, "%lu %lu", &total, &resident);
    std::fclose(f);
    if (matched < 2) {
        return 0;
    }
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        pageSize = 4096;
    }
    return (resident * static_cast<std::size_t>(pageSize)) / 1024;
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
    int maxFootprintDeltaKb = 0;
    auto parsePositiveInt = [](const QString& value, const char* option, int& output) -> bool {
        bool parsed = false;
        const int parsedValue = value.toInt(&parsed);
        if (!parsed || parsedValue <= 0) {
            std::fprintf(stderr, "error: %s must be a positive integer\n", option);
            return false;
        }
        output = parsedValue;
        return true;
    };

    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a == "--db" && i + 1 < args.size()) {
            dbPath = args[++i];
        } else if (a == "--pages" && i + 1 < args.size()) {
            if (!parsePositiveInt(args[++i], "--pages", totalPages)) {
                return 1;
            }
        } else if (a == "--page-size" && i + 1 < args.size()) {
            if (!parsePositiveInt(args[++i], "--page-size", pageSize)) {
                return 1;
            }
        } else if (a == "--max-footprint-delta-kb" && i + 1 < args.size()) {
            if (!parsePositiveInt(args[++i], "--max-footprint-delta-kb", maxFootprintDeltaKb)) {
                return 1;
            }
        }
    }
    if (dbPath.isEmpty()) {
        std::fprintf(stderr, "usage: ssa_mem_stress --db <path> [--pages N] [--page-size N] "
                             "[--max-footprint-delta-kb N]\n");
        return 1;
    }

    const auto dbFilePath = ssa::qt::toFileSystemPath(dbPath);
    std::error_code ec;
    if (!std::filesystem::exists(dbFilePath, ec)) {
        const auto displayPath = ssa::qt::toUtf8(dbFilePath);
        std::fprintf(stderr, "error: database file does not exist: %s\n", displayPath.c_str());
        return 1;
    }

    auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(dbFilePath);
    auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
    ssa::presentation::BrowseViewModel browse(service);

    // Persistent connection tracks pageChanged emissions across waitPage calls
    // so a signal fired while the event loop is not spinning is still captured.
    // waitPage spins a nested event loop with a timeout; the persistent lambda
    // sets the flag and quits the loop. No temporary connection is created, so
    // there is no race window between connect and loop.exec().
    bool pageChangedEmitted = false;
    QEventLoop* activeLoop = nullptr;
    QObject::connect(&browse, &ssa::presentation::BrowseViewModel::pageChanged, [&]() {
        pageChangedEmitted = true;
        if (activeLoop != nullptr) {
            activeLoop->quit();
        }
    });

    // Returns true if pageChanged fired within the timeout, false on timeout.
    auto waitPage = [&]() -> bool {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (pageChangedEmitted) {
            pageChangedEmitted = false;
            return true;
        }
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(5000);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        activeLoop = &loop;
        timeoutTimer.start();
        loop.exec();
        activeLoop = nullptr;
        const bool received = pageChangedEmitted;
        pageChangedEmitted = false;
        return received;
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
    const auto startupFootprintKb = currentFootprintKb();
    std::printf("phase\tpages\tfootprint_kb\n");
    std::printf("startup\t0\t%zu\n", startupFootprintKb);
    std::printf("firstpage\t1\t%zu\n", currentFootprintKb());

    int lastPageNumber = browse.pageNumber();
    for (int i = 2; i <= totalPages; ++i) {
        const int pageCountBefore = browse.pageCount();
        const int pageNumberBefore = browse.pageNumber();
        // Guard against single-page or empty result sets: navigating next would
        // be a no-op or wrap, and the reset branch would loop forever.
        if (pageCountBefore <= 1) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            pageChangedEmitted = false;
        } else if (pageNumberBefore >= pageCountBefore) {
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
        // Recapture pageCount after the load: clearSearchAndResetPage/nextPage
        // can change totalRows and therefore pageCount, so the value captured
        // before navigation is stale for the progress check.
        const int currentPage = browse.pageNumber();
        const int currentPageCount = browse.pageCount();
        // Guard against the model failing to advance pages, which would make
        // the loop spin without progress and waste resources. Only enforce
        // when there is more than one page to advance through.
        if (currentPageCount > 1 && currentPage == lastPageNumber) {
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
    const auto finalFootprintKb = currentFootprintKb();
    std::printf("final\t%d\t%zu\n", totalPages, finalFootprintKb);

    if (maxFootprintDeltaKb > 0) {
        if (startupFootprintKb == 0 || finalFootprintKb == 0) {
            std::fprintf(stderr, "error: footprint sampling failed\n");
            return 2;
        }
        const auto footprintDeltaKb =
            finalFootprintKb > startupFootprintKb ? finalFootprintKb - startupFootprintKb : 0;
        std::printf("budget\t%zu\t%d\n", footprintDeltaKb, maxFootprintDeltaKb);
        if (footprintDeltaKb > static_cast<std::size_t>(maxFootprintDeltaKb)) {
            std::fprintf(stderr, "error: footprint delta %zu KB exceeds budget %d KB\n",
                         footprintDeltaKb, maxFootprintDeltaKb);
            return 2;
        }
    }

    return 0;
}
