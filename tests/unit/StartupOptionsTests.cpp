#include "platform/DesktopExternalCommandPort.h"
#include "platform/OpenPathPolicy.h"
#include "platform/SamUrlBuilder.h"
#include "platform/StartupOptions.h"
#include "ports/IExternalCommandPort.h"

#include <catch2/catch_test_macros.hpp>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

    std::string processIdStamp() {
#ifdef _WIN32
        const auto pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
        const auto pid = static_cast<unsigned long>(getpid());
#endif
        return std::to_string(pid);
    }

    std::string uniqueStamp() {
        static std::atomic<unsigned long> counter{0};
        const auto seq = counter.fetch_add(1, std::memory_order_relaxed);
        std::random_device rd;
        return processIdStamp() + "-" + std::to_string(seq) + "-" + std::to_string(rd());
    }

    class TemporaryDirectoryPair final {
      public:
        TemporaryDirectoryPair()
            : root{std::filesystem::temp_directory_path() /
                   ("ssa-open-policy-root-" + uniqueStamp())},
              outside{std::filesystem::temp_directory_path() /
                      ("ssa-open-policy-outside-" + uniqueStamp())} {
            std::filesystem::create_directories(root);
            std::filesystem::create_directories(outside);
        }

        ~TemporaryDirectoryPair() {
            // Best-effort cleanup; ignore errors so a parallel test removing a
            // sibling path does not abort this process via throwing destructor.
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
            std::filesystem::remove_all(outside, ec);
        }

        TemporaryDirectoryPair(const TemporaryDirectoryPair&) = delete;
        TemporaryDirectoryPair& operator=(const TemporaryDirectoryPair&) = delete;

        std::filesystem::path root;
        std::filesystem::path outside;
    };

    void addStartupOptions(QCommandLineParser& parser) {
        parser.addOption(QCommandLineOption(QStringList{"project-root"}, "", "path"));
        parser.addOption(
            QCommandLineOption(QStringList{"db", "database", "database-path"}, "", "path"));
        parser.addOption(QCommandLineOption(QStringList{"config-dir"}, "", "path"));
        parser.addOption(QCommandLineOption(QStringList{"sam-url"}, "", "url"));
    }

    QString testDatabasePath() {
        return QDir::current().filePath("startup-options-test.db");
    }

    class CurrentDirectoryGuard final {
      public:
        explicit CurrentDirectoryGuard(QString nextPath) : previousPath_{QDir::currentPath()} {
            if (!QDir::setCurrent(nextPath)) {
                throw std::runtime_error("cannot change current directory for test");
            }
        }

        ~CurrentDirectoryGuard() {
            QDir::setCurrent(previousPath_);
        }

        CurrentDirectoryGuard(const CurrentDirectoryGuard&) = delete;
        CurrentDirectoryGuard& operator=(const CurrentDirectoryGuard&) = delete;

      private:
        QString previousPath_;
    };

} // namespace

TEST_CASE("startup options accept configured absolute SAM URL") {
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse({"ssa_test", "--project-root", QDir::currentPath(), "--db",
                          testDatabasePath(), "--sam-url", "https://example.invalid"}));

    const ssa::platform::StartupOptions options = ssa::platform::StartupOptions::fromParser(parser);

    REQUIRE(options.samBaseUrl == "https://example.invalid");
}

TEST_CASE("startup options use SAM default URL when omitted") {
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse(
        {"ssa_test", "--project-root", QDir::currentPath(), "--db", testDatabasePath()}));

    const ssa::platform::StartupOptions options = ssa::platform::StartupOptions::fromParser(parser);

    REQUIRE(options.samBaseUrl == ssa::platform::StartupOptions::defaultSamBaseUrl());
}

TEST_CASE("startup options discover project root from current directory database") {
    const TemporaryDirectoryPair directories;
    const auto dataDirectory = directories.root / "data";
    std::filesystem::create_directories(dataDirectory);
    {
        std::ofstream database{dataDirectory / "ssas.db"};
    }
    const auto nestedDirectory = directories.root / "build" / "release";
    std::filesystem::create_directories(nestedDirectory);

    const CurrentDirectoryGuard currentDirectory{QString::fromStdString(nestedDirectory.string())};
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse({"ssa_test"}));

    const ssa::platform::StartupOptions options = ssa::platform::StartupOptions::fromParser(parser);

    REQUIRE(options.projectRoot ==
            QString::fromStdString(std::filesystem::weakly_canonical(directories.root).string()));
    REQUIRE(options.databasePath ==
            QString::fromStdString(
                std::filesystem::weakly_canonical(dataDirectory / "ssas.db").string()));
}

TEST_CASE("startup options accept database path aliases") {
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse({"ssa_test", "--project-root", QDir::currentPath(), "--database-path",
                          testDatabasePath()}));

    const ssa::platform::StartupOptions options = ssa::platform::StartupOptions::fromParser(parser);

    REQUIRE(options.databasePath ==
            QString::fromStdString(std::filesystem::absolute(testDatabasePath().toStdString())
                                       .lexically_normal()
                                       .string()));
}

TEST_CASE("startup options reject invalid SAM URL") {
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse({"ssa_test", "--project-root", QDir::currentPath(), "--db",
                          testDatabasePath(), "--sam-url", "not a url"}));

    REQUIRE_THROWS_AS(ssa::platform::StartupOptions::fromParser(parser), std::invalid_argument);
}

TEST_CASE("startup options reject non web SAM URL schemes") {
    QCommandLineParser parser;
    addStartupOptions(parser);
    REQUIRE(parser.parse({"ssa_test", "--project-root", QDir::currentPath(), "--db",
                          testDatabasePath(), "--sam-url", "file:///tmp/sam"}));

    REQUIRE_THROWS_AS(ssa::platform::StartupOptions::fromParser(parser), std::invalid_argument);
}

TEST_CASE("SAM URL builder uses public view contract") {
    const auto url = ssa::platform::SamUrlBuilder::publicSsaUrl(
        QUrl{"https://osprd.itaipu/SAM_SMA/"}, "202600001");

    REQUIRE(url.toString() ==
            "https://osprd.itaipu/SAM_SMA/SSAPublicView.aspx?SerialNumber=202600001&language=pt");
}

TEST_CASE("SAM URL builder preserves configured path prefix") {
    const auto url = ssa::platform::SamUrlBuilder::publicSsaUrl(
        QUrl{"https://example.invalid/custom/sam"}, "202600001");

    REQUIRE(
        url.toString() ==
        "https://example.invalid/custom/sam/SSAPublicView.aspx?SerialNumber=202600001&language=pt");
}

TEST_CASE("open path policy rejects paths outside allowed roots") {
    const TemporaryDirectoryPair directories;
    const auto insideFile = directories.root / "child.txt";
    const auto outsideFile = directories.outside / "child.txt";
    {
        std::ofstream inside{insideFile};
        std::ofstream external{outsideFile};
    }

    const ssa::platform::OpenPathPolicy policy({directories.root});

    const auto accepted = policy.validate(insideFile.string());
    const auto rejected = policy.validate(outsideFile.string());

    REQUIRE(accepted.status == ssa::ports::ExternalCommandStatus::Succeeded);
    REQUIRE(rejected.status == ssa::ports::ExternalCommandStatus::Rejected);
}

TEST_CASE("open path policy rejects filesystem root as allowed root") {
    const TemporaryDirectoryPair directories;
    const auto insideFile = directories.root / "child.txt";
    {
        std::ofstream inside{insideFile};
    }

    const ssa::platform::OpenPathPolicy policy({insideFile.root_path()});

    const auto rejected = policy.validate(insideFile.string());

    REQUIRE(rejected.status == ssa::ports::ExternalCommandStatus::Rejected);
}

TEST_CASE("open path policy accepts new paths under existing allowed roots") {
    const TemporaryDirectoryPair directories;
    const ssa::platform::OpenPathPolicy policy({directories.root});

    const auto accepted = policy.validate((directories.root / "new-file.txt").string());
    const auto acceptedNested =
        policy.validate((directories.root / "missing" / "new-file.txt").string());
    const auto rejectedEscape =
        policy.validate((directories.root / "missing" / ".." / ".." / "blocked.txt").string());

    REQUIRE(accepted.status == ssa::ports::ExternalCommandStatus::Succeeded);
    REQUIRE(acceptedNested.status == ssa::ports::ExternalCommandStatus::Succeeded);
    REQUIRE(rejectedEscape.status == ssa::ports::ExternalCommandStatus::Rejected);
}

TEST_CASE("desktop external command port rejects worker thread before dispatch") {
    ssa::platform::DesktopExternalCommandPort commands(QUrl{"https://example.invalid/sam"});
    ssa::ports::ExternalCommandResult result;
    const auto executeInWorker = [&] {
        std::thread worker([&] {
            result = commands.execute(
                ssa::ports::ExternalCommand{ssa::ports::ExternalCommandKind::OpenSamHome, {}});
        });
        worker.join();
    };
    if (QCoreApplication::instance() == nullptr) {
        int argc = 1;
        std::array<char, 15> applicationName{"ssa-unit-tests"};
        std::array<char*, 2> arguments{applicationName.data(), nullptr};
        QCoreApplication application(argc, arguments.data());
        executeInWorker();
    } else {
        executeInWorker();
    }

    REQUIRE(result.status == ssa::ports::ExternalCommandStatus::Rejected);
    REQUIRE(result.message == "external commands must run on the GUI thread");
}
