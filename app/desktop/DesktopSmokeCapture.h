#pragma once

#include <QObject>

class QCommandLineParser;
class QQmlApplicationEngine;

namespace ssa::app::desktop {

    class DesktopSmokeController final : public QObject {
        Q_OBJECT

      public:
        explicit DesktopSmokeController(QObject* parent = nullptr);
        void requestOpenPreferences();
        void requestOpenAdvancedFilters();
        void requestOpenAdvancedPopup();
        void requestOpenDetailsWindow();

      signals:
        void openPreferencesRequested();
        void openAdvancedFiltersRequested();
        void openAdvancedPopupRequested();
        void openDetailsWindowRequested();
    };

    class DesktopSmokeCapture final {
      public:
        static void installIfRequested(const QCommandLineParser& parser,
                                       QQmlApplicationEngine& engine,
                                       DesktopSmokeController& controller);
    };

} // namespace ssa::app::desktop
