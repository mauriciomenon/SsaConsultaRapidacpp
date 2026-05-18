#include "platform/SamUrlBuilder.h"
#include "platform/StartupOptions.h"

#include <catch2/catch_test_macros.hpp>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>

#include <stdexcept>

namespace {

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
