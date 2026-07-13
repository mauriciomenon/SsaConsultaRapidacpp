#include "DesktopApplicationRuntime.h"

#include "DesktopMainViewModelFactory.h"
#include "platform/SupervisedProcess.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QVariant>

#include <cstdlib>

namespace ssa::app::desktop {

    DesktopApplicationRuntime::DesktopApplicationRuntime(const QCommandLineParser& parser)
        : options_(ssa::platform::StartupOptions::fromParser(parser)),
          paths_(options_.projectRoot, options_.configDir) {
        paths_.ensureConfigDirectory();
        mainViewModel_ = DesktopMainViewModelFactory::create(options_, paths_);
        QObject::connect(mainViewModel_->databaseSwitch(),
                         &ssa::presentation::DatabaseSwitchViewModel::replacementStarted,
                         QCoreApplication::instance(), &QCoreApplication::quit,
                         Qt::QueuedConnection);
        QObject::connect(mainViewModel_.get(),
                         &ssa::presentation::MainViewModel::forcedShutdownRequested,
                         QCoreApplication::instance(), &DesktopApplicationRuntime::forceShutdown);
        QObject::connect(&systemThemeResolver_,
                         &ssa::platform::SystemThemeResolver::systemThemeChanged,
                         mainViewModel_->ui(), [ui = mainViewModel_->ui(), this] {
                             ui->setSystemTheme(systemThemeResolver_.currentTheme());
                         });
        mainViewModel_->ui()->setSystemTheme(systemThemeResolver_.currentTheme());
    }

    void DesktopApplicationRuntime::forceShutdown() {
        const auto status = ssa::platform::SupervisedProcess::requestForceStopAll();
        if (status == ssa::platform::ForceStopRequestStatus::PendingStart) {
            QTimer::singleShot(25, QCoreApplication::instance(),
                               &DesktopApplicationRuntime::forceShutdown);
            return;
        }
        if (status == ssa::platform::ForceStopRequestStatus::Failed) {
            qCritical() << "Forced shutdown could not signal process cleanup";
        }
        std::_Exit(status == ssa::platform::ForceStopRequestStatus::Ready ? EXIT_SUCCESS
                                                                          : EXIT_FAILURE);
    }

    void DesktopApplicationRuntime::loadMainWindow(QQmlApplicationEngine& engine) {
        engine.setInitialProperties({{"mainViewModel", QVariant::fromValue(mainViewModel_.get())},
                                     {"smokeController", QVariant::fromValue(&smokeController_)}});
        engine.loadFromModule("SsaConsultaRapida", "Main");
    }

    void DesktopApplicationRuntime::installSmokeCapture(const QCommandLineParser& parser,
                                                        QQmlApplicationEngine& engine) {
        DesktopSmokeCapture::installIfRequested(parser, engine, smokeController_);
    }

} // namespace ssa::app::desktop
