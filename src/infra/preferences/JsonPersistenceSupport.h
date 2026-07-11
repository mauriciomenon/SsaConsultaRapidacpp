#pragma once

#include <QByteArray>
#include <QFile>
#include <QJsonObject>
#include <QSaveFile>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ssa::infra::preferences::json_persistence {

    inline constexpr qint64 kMaxJsonFileBytes = 1024LL * 1024LL;

    [[nodiscard]] inline std::string errorMessage(const std::string_view operation,
                                                  const std::string_view subject,
                                                  const std::filesystem::path& path) {
        return std::string{operation} + " " + std::string{subject} + ": " + path.string();
    }

    [[nodiscard]] inline QByteArray readBounded(const std::filesystem::path& path,
                                                const std::string_view subject) {
        QFile input(QString::fromStdString(path.string()));
        if (!input.open(QIODevice::ReadOnly)) {
            throw std::runtime_error(errorMessage("cannot read", subject, path));
        }

        QByteArray payload = input.read(kMaxJsonFileBytes + 1);
        if (input.error() != QFileDevice::NoError) {
            throw std::runtime_error(errorMessage("cannot read", subject, path) + ": " +
                                     input.errorString().toStdString());
        }
        if (payload.size() > kMaxJsonFileBytes || !input.atEnd()) {
            throw std::runtime_error(errorMessage("size limit exceeded for", subject, path));
        }
        return payload;
    }

    inline void writeFully(QIODevice& output, const QByteArray& payload,
                           const std::string_view subject, const std::filesystem::path& path) {
        qint64 offset = 0;
        while (offset < payload.size()) {
            const qint64 written =
                output.write(payload.constData() + offset, payload.size() - offset);
            if (written <= 0) {
                throw std::runtime_error(errorMessage("cannot write", subject, path) + ": " +
                                         output.errorString().toStdString());
            }
            offset += written;
        }
    }

    inline void commitOrThrow(QSaveFile& output, const std::string_view subject,
                              const std::filesystem::path& path) {
        if (!output.commit()) {
            throw std::runtime_error(errorMessage("cannot commit", subject, path) + ": " +
                                     output.errorString().toStdString());
        }
    }

    inline void writeAtomically(const std::filesystem::path& path, const QByteArray& payload,
                                const std::string_view subject) {
        if (payload.size() > kMaxJsonFileBytes) {
            throw std::runtime_error(errorMessage("size limit exceeded for", subject, path));
        }

        QSaveFile output(QString::fromStdString(path.string()));
        output.setDirectWriteFallback(false);
        if (!output.open(QIODevice::WriteOnly)) {
            throw std::runtime_error(errorMessage("cannot write", subject, path));
        }

        writeFully(output, payload, subject, path);
        commitOrThrow(output, subject, path);
    }

    [[nodiscard]] inline int schemaVersion(const QJsonObject& root, const int currentVersion,
                                           const std::string_view subject) {
        const auto value = root.value("schema_version");
        if (!value.isDouble()) {
            throw std::runtime_error("missing or invalid schema_version in " +
                                     std::string{subject});
        }
        const double rawVersion = value.toDouble();
        if (rawVersion > currentVersion) {
            throw std::runtime_error("unsupported future schema_version in " +
                                     std::string{subject});
        }
        if (rawVersion < 1.0 || rawVersion != static_cast<int>(rawVersion)) {
            throw std::runtime_error("invalid schema_version in " + std::string{subject});
        }
        return static_cast<int>(rawVersion);
    }

} // namespace ssa::infra::preferences::json_persistence
