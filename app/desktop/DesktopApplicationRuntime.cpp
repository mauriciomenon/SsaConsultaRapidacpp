#include "DesktopApplicationRuntime.h"

#include "DesktopMainViewModelFactory.h"

#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QVariant>

namespace ssa::app::desktop {

    DesktopApplicationRuntime::DesktopApplicationRuntime(const QCommandLineParser& parser)
        : options_(ssa::platform::StartupOptions::fromParser(parser)),
          paths_(options_.projectRoot, options_.configDir) {
        paths_.ensureConfigDirectory();
        mainViewModel_ = DesktopMainViewModelFactory::create(options_, paths_);
        QObject::connect(&systemThemeResolver_,
                         &ssa::platform::SystemThemeResolver::systemThemeChanged,
                         mainViewModel_->ui(), [ui = mainViewModel_->ui(), this] {
                             ui->setSystemTheme(systemThemeResolver_.currentTheme());
                         });
        mainViewModel_->ui()->setSystemTheme(systemThemeResolver_.currentTheme());
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
