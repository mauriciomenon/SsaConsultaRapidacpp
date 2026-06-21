#include "platform/OpenPathPolicy.h"
#include "platform/SamUrlBuilder.h"
#include "platform/StartupOptions.h"
#include "ports/IExternalCommandPort.h"

#include <catch2/catch_test_macros.hpp>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

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
        parser.addOption(QCommandLineOption(QStringList{"db"}, "", "path"));
        parser.addOption(QCommandLineOption(QStringList{"config-dir"}, "", "path"));
        parser.addOption(QCommandLineOption(QStringList{"sam-url"}, "", "url"));
    }

    QString testDatabasePath() {
        return QDir::current().filePath("startup-options-test.db");
    }

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
