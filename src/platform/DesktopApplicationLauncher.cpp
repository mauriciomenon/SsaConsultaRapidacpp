#include "platform/DesktopApplicationLauncher.h"

#include "qt/FilesystemPath.h"

#include <QCoreApplication>
#include <QProcess>

namespace ssa::platform {

    namespace {

        ports::ApplicationLaunchResult launchCurrentExecutable(const QStringList& arguments) {
            const auto executable = QCoreApplication::applicationFilePath();
            if (executable.isEmpty()) {
                return {false, "O executavel atual nao esta disponivel"};
            }

            qint64 processId = 0;
            const bool started = QProcess::startDetached(executable, arguments, {}, &processId);
            if (!started || processId <= 0) {
                return {false, "A nova instancia nao iniciou"};
            }
            return {true, {}};
        }

    } // namespace

    DesktopApplicationLauncher::DesktopApplicationLauncher(const StartupOptions& options)
        : persistentArguments_{QStringLiteral("--project-root"), options.projectRoot,
                               QStringLiteral("--config-dir"),   options.configDir,
                               QStringLiteral("--sam-url"),      options.samBaseUrl},
          samBaseUrl_(options.samBaseUrl) {}

    ports::ApplicationLaunchResult
    DesktopApplicationLauncher::launchWithDatabase(const std::filesystem::path& path) {
        return launchCurrentExecutable(argumentsForDatabase(path));
    }

    ports::ApplicationLaunchResult
    DesktopApplicationLauncher::launchConfigured(const ports::ApplicationLaunchTargets& targets) {
        return launchCurrentExecutable(argumentsForConfigured(targets));
    }

    QStringList
    DesktopApplicationLauncher::argumentsForDatabase(const std::filesystem::path& path) const {
        auto arguments = persistentArguments_;
        arguments.append({QStringLiteral("--db"), qt::toQString(path)});
        return arguments;
    }

    QStringList DesktopApplicationLauncher::argumentsForConfigured(
        const ports::ApplicationLaunchTargets& targets) const {
        return {QStringLiteral("--project-root"), qt::toQString(targets.projectRoot),
                QStringLiteral("--config-dir"),   qt::toQString(targets.configDir),
                QStringLiteral("--sam-url"),      samBaseUrl_,
                QStringLiteral("--db"),           qt::toQString(targets.databasePath)};
    }

} // namespace ssa::platform
