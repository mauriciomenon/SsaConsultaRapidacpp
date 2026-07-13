#include "platform/DesktopApplicationLauncher.h"

#include "qt/FilesystemPath.h"

#include <QCoreApplication>
#include <QProcess>

namespace ssa::platform {

    DesktopApplicationLauncher::DesktopApplicationLauncher(const StartupOptions& options)
        : persistentArguments_{QStringLiteral("--project-root"), options.projectRoot,
                               QStringLiteral("--config-dir"),   options.configDir,
                               QStringLiteral("--sam-url"),      options.samBaseUrl} {}

    ports::ApplicationLaunchResult
    DesktopApplicationLauncher::launchWithDatabase(const std::filesystem::path& path) {
        const auto executable = QCoreApplication::applicationFilePath();
        if (executable.isEmpty()) {
            return {false, "O executavel atual nao esta disponivel"};
        }

        qint64 processId = 0;
        const bool started =
            QProcess::startDetached(executable, argumentsForDatabase(path), {}, &processId);
        if (!started || processId <= 0) {
            return {false, "A nova instancia nao iniciou"};
        }
        return {true, {}};
    }

    QStringList
    DesktopApplicationLauncher::argumentsForDatabase(const std::filesystem::path& path) const {
        auto arguments = persistentArguments_;
        arguments.append({QStringLiteral("--db"), qt::toQString(path)});
        return arguments;
    }

} // namespace ssa::platform
