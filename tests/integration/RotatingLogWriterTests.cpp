#include "platform/RotatingLogWriter.h"

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
