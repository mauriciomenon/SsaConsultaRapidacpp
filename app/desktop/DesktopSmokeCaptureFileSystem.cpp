#include "DesktopSmokeCaptureFileSystem.h"

#include <QDir>
#include <QFileInfo>

namespace ssa::app::desktop {

    QString DesktopSmokeCaptureFileSystem::absoluteScreenshotPath(const QString& screenshotPath) {
        return QFileInfo{screenshotPath}.absoluteFilePath();
    }

    bool DesktopSmokeCaptureFileSystem::ensureScreenshotDirectory(const QString& screenshotPath) {
        const QFileInfo fileInfo{absoluteScreenshotPath(screenshotPath)};
        const QDir parent = fileInfo.absoluteDir();
        return QDir{}.mkpath(parent.absolutePath());
    }

} // namespace ssa::app::desktop
