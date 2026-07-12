#pragma once

#include <QObject>
#include <QVariantMap>

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
        void requestLayoutProbe();
        Q_INVOKABLE void reportCaptureFailure();
        Q_INVOKABLE void reportDetailsReady();
        Q_INVOKABLE void reportAdvancedPopupMetrics(const QVariantMap& metrics);
        Q_INVOKABLE void reportLayoutMetrics(const QVariantMap& metrics);

      signals:
        void openPreferencesRequested();
        void openAdvancedFiltersRequested();
        void openAdvancedPopupRequested();
        void openDetailsWindowRequested();
        void layoutProbeRequested();
        void captureFailureReported();
        void detailsReady();
        void advancedPopupMetricsReady(QVariantMap metrics);
        void layoutMetricsReady(QVariantMap metrics);
    };

    class DesktopSmokeCapture final {
      public:
        static void installIfRequested(const QCommandLineParser& parser,
                                       QQmlApplicationEngine& engine,
                                       DesktopSmokeController& controller);
    };

} // namespace ssa::app::desktop
