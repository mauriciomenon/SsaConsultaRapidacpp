#include "DesktopApplicationRuntime.h"

#include "DesktopLogSink.h"
#include "DesktopMainViewModelFactory.h"
#include "diagnostics/StartupTrace.h"
#include "platform/SupervisedProcess.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <cstdlib>

namespace ssa::app::desktop {

    DesktopApplicationRuntime::DesktopApplicationRuntime(const QCommandLineParser& parser)
        : options_(ssa::platform::StartupOptions::fromParser(parser)),
          paths_(options_.projectRoot, options_.configDir) {
        ssa::diagnostics::traceStartupEvent("runtime_start", "thread=gui");
        paths_.ensureConfigDirectory();
        mainViewModel_ = DesktopMainViewModelFactory::create(options_, paths_);
        logSink_ =
            std::make_unique<DesktopLogSink>(paths_.configDirectoryPath(), *mainViewModel_->logs());
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

    DesktopApplicationRuntime::~DesktopApplicationRuntime() = default;

    void DesktopApplicationRuntime::forceShutdown() {
        constexpr qint64 forceShutdownTimeoutMs = 5'000;
        static QElapsedTimer drainTimer;
        if (!drainTimer.isValid()) {
            drainTimer.start();
        }
        const auto status = ssa::platform::SupervisedProcess::requestForceStopAll();
        if (status == ssa::platform::ForceStopRequestStatus::Drained) {
            std::_Exit(EXIT_SUCCESS);
        }
        if (status == ssa::platform::ForceStopRequestStatus::Failed && drainTimer.elapsed() < 25) {
            qCritical() << "Forced shutdown could not signal process cleanup";
        }
        if (drainTimer.elapsed() >= forceShutdownTimeoutMs) {
            qCritical() << "Forced shutdown timed out before process drain";
            std::_Exit(EXIT_FAILURE);
        }
        QTimer::singleShot(25, QCoreApplication::instance(),
                           &DesktopApplicationRuntime::forceShutdown);
    }

    void DesktopApplicationRuntime::loadMainWindow(QQmlApplicationEngine& engine) {
        engine.setInitialProperties({{"mainViewModel", QVariant::fromValue(mainViewModel_.get())},
                                     {"smokeController", QVariant::fromValue(&smokeController_)}});
        QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreated, &engine,
            [](QObject* object, const QUrl&) {
                ssa::diagnostics::traceStartupEvent(
                    "qml_object_created", object == nullptr ? "thread=gui outcome=failure"
                                                            : "thread=gui outcome=success");
                auto* window = qobject_cast<QQuickWindow*>(object);
                if (window == nullptr) {
                    return;
                }
                QObject::connect(
                    window, &QQuickWindow::frameSwapped, window,
                    [] {
                        ssa::diagnostics::traceStartupEvent("first_frame_swapped", "thread=gui");
                    },
                    Qt::SingleShotConnection);
            },
            Qt::SingleShotConnection);
        engine.loadFromModule("SsaConsultaRapida", "Main");
    }

    void DesktopApplicationRuntime::installSmokeCapture(const QCommandLineParser& parser,
                                                        QQmlApplicationEngine& engine) {
        DesktopSmokeCapture::installIfRequested(parser, engine, smokeController_);
    }

} // namespace ssa::app::desktop
