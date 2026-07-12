#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtGlobal>

#include <filesystem>
#include <string>

namespace ssa::qt {

    [[nodiscard]] inline std::filesystem::path toFileSystemPath(const QString& value) {
#ifdef Q_OS_WIN
        return std::filesystem::path{value.toStdWString()};
#else
        const auto encoded = QFile::encodeName(value);
        return std::filesystem::path{
            std::string{encoded.constData(), static_cast<std::size_t>(encoded.size())}};
#endif
    }

    [[nodiscard]] inline QString toQString(const std::filesystem::path& value) {
#ifdef Q_OS_WIN
        return QString::fromStdWString(value.native());
#else
        const auto& native = value.native();
        return QFile::decodeName(QByteArray{native.data(), static_cast<qsizetype>(native.size())});
#endif
    }

    [[nodiscard]] inline std::string toUtf8(const std::filesystem::path& value) {
        const auto encoded = toQString(value).toUtf8();
        return {encoded.constData(), static_cast<std::size_t>(encoded.size())};
    }

} // namespace ssa::qt
