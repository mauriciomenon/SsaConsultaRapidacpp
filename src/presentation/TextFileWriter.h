#pragma once

#include <QByteArray>
#include <QSaveFile>
#include <QString>

namespace ssa::presentation {

    // Writes UTF-8 text through a temporary file, so a failed write never leaves a partial file.
    [[nodiscard]] inline bool writeTextFileAtomically(const QString& path, const QString& content) {
        if (path.trimmed().isEmpty()) {
            return false;
        }
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        const QByteArray payload = content.toUtf8();
        if (file.write(payload) != payload.size()) {
            file.cancelWriting();
            return false;
        }
        return file.commit();
    }

} // namespace ssa::presentation
