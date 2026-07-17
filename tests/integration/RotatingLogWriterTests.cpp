#include "platform/RotatingLogWriter.h"

#include <QFile>
#include <QStringDecoder>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("rotating log writer bounds the number and size of log files") {
    QTemporaryDir temporaryDirectory;
    REQUIRE(temporaryDirectory.isValid());
    const auto path = std::filesystem::path{temporaryDirectory.path().toStdString()} / "ssa.log";
    ssa::platform::RotatingLogWriter writer(path, 64, 3);

    for (int index = 0; index < 20; ++index) {
        writer.append("event-" + std::to_string(index) + "-diagnostic");
    }

    REQUIRE(std::filesystem::exists(path));
    REQUIRE(std::filesystem::exists(path.string() + ".1"));
    REQUIRE(std::filesystem::exists(path.string() + ".2"));
    REQUIRE_FALSE(std::filesystem::exists(path.string() + ".3"));
    REQUIRE(std::filesystem::file_size(path) <= 64);
    REQUIRE(std::filesystem::file_size(path.string() + ".1") <= 64);
    REQUIRE(std::filesystem::file_size(path.string() + ".2") <= 64);
}

TEST_CASE("rotating log writer serializes concurrent append and rotation") {
    QTemporaryDir temporaryDirectory;
    REQUIRE(temporaryDirectory.isValid());
    const auto path = std::filesystem::path{temporaryDirectory.path().toStdString()} / "ssa.log";
    ssa::platform::RotatingLogWriter writer(path, 256, 3);
    std::vector<std::jthread> workers;

    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&writer, worker] {
            for (int index = 0; index < 50; ++index) {
                writer.append("worker-" + std::to_string(worker) + "-event-" +
                              std::to_string(index));
            }
        });
    }
    workers.clear();

    REQUIRE(std::filesystem::file_size(path) <= 256);
    REQUIRE(std::filesystem::file_size(path.string() + ".1") <= 256);
    REQUIRE(std::filesystem::file_size(path.string() + ".2") <= 256);
}

TEST_CASE("rotating log writer truncates only at a valid UTF-8 boundary") {
    QTemporaryDir temporaryDirectory;
    REQUIRE(temporaryDirectory.isValid());
    const auto path = std::filesystem::path{temporaryDirectory.path().toStdString()} / "ssa.log";

    SECTION("minimum accepted limit") {
        ssa::platform::RotatingLogWriter writer(path, 17, 1);
        writer.append("abcdefghijklmnopq");

        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray content = file.readAll();
        REQUIRE(content.endsWith("... [truncated]\n"));
        REQUIRE(content.size() <= 17);
    }

    SECTION("ASCII payload") {
        ssa::platform::RotatingLogWriter writer(path, 24, 1);
        writer.append("abcdefghijklmnopqrstuvwxyz");

        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::ReadOnly));
        REQUIRE(file.readAll() == "abcdefgh... [truncated]\n");
    }

    SECTION("exact limit") {
        ssa::platform::RotatingLogWriter writer(path, 20, 1);
        writer.append("abcdefghijklmnopqrs");

        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::ReadOnly));
        REQUIRE(file.readAll() == "abcdefghijklmnopqrs\n");
    }

    SECTION("four byte code point") {
        ssa::platform::RotatingLogWriter writer(path, 42, 1);
        const std::string codePoint{"\xF0\x9F\x98\x80"};
        std::string payload;
        for (int index = 0; index < 12; ++index) {
            payload += codePoint;
        }

        writer.append(payload);

        QFile file(QString::fromStdString(path.string()));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray content = file.readAll();
        REQUIRE(content.endsWith("... [truncated]\n"));
        QStringDecoder decoder{QStringDecoder::Utf8};
        const QString decoded = decoder.decode(content) + decoder.decode({});
        REQUIRE_FALSE(decoder.hasError());
        std::string expectedPayload;
        for (int index = 0; index < 6; ++index) {
            expectedPayload += codePoint;
        }
        expectedPayload += "... [truncated]\n";
        REQUIRE(decoded == QString::fromUtf8(expectedPayload));
    }
}
