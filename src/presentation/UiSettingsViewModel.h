#pragma once

#include "ports/IUserPreferencesStore.h"
#include "ports/UserPreferenceDefaults.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace ssa::presentation {

    class UiSettingsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
        Q_PROPERTY(QString resolvedTheme READ resolvedTheme NOTIFY resolvedThemeChanged)
        Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY densityChanged)
        Q_PROPERTY(bool detailsVisible READ detailsVisible WRITE setDetailsVisible NOTIFY
                       detailsVisibleChanged)
        Q_PROPERTY(int detailsPanelWidth READ detailsPanelWidth WRITE setDetailsPanelWidth NOTIFY
                       detailsPanelWidthChanged)
        Q_PROPERTY(int detailsViewportWidth READ detailsViewportWidth WRITE setDetailsViewportWidth
                       NOTIFY detailsWidthLayoutChanged)
        Q_PROPERTY(
            int detailsMinimumWidth READ detailsMinimumWidth NOTIFY detailsWidthLayoutChanged)
        Q_PROPERTY(
            int detailsPreferredWidth READ detailsPreferredWidth NOTIFY detailsWidthLayoutChanged)
        Q_PROPERTY(
            int detailsMaximumWidth READ detailsMaximumWidth NOTIFY detailsWidthLayoutChanged)
        Q_PROPERTY(
            int detailsEffectiveWidth READ detailsEffectiveWidth NOTIFY detailsWidthLayoutChanged)

      public:
        explicit UiSettingsViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString theme() const;
        void setTheme(const QString& value);
        [[nodiscard]] QString resolvedTheme() const;
        [[nodiscard]] QString density() const;
        void setDensity(const QString& value);
        [[nodiscard]] bool detailsVisible() const;
        void setDetailsVisible(bool value);
        [[nodiscard]] int detailsPanelWidth() const;
        void setDetailsPanelWidth(int value);
        [[nodiscard]] int detailsViewportWidth() const;
        void setDetailsViewportWidth(int value);
        [[nodiscard]] int detailsMinimumWidth() const;
        [[nodiscard]] int detailsPreferredWidth() const;
        [[nodiscard]] int detailsMaximumWidth() const;
        [[nodiscard]] int detailsEffectiveWidth() const;

        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

      signals:
        void themeChanged();
        void resolvedThemeChanged();
        void densityChanged();
        void detailsVisibleChanged();
        void detailsPanelWidthChanged();
        void settingsChanged();
        void detailsWidthLayoutChanged();
        void preferencesSaveRequested();

      private:
        [[nodiscard]] bool isDensityValid(const QString& value) const;
        [[nodiscard]] int clampToWidthRange(int value) const;
        [[nodiscard]] int computeDetailsMinimumWidth() const;
        [[nodiscard]] int computeDetailsMaximumWidth() const;
        [[nodiscard]] int computeDetailsPreferredWidth() const;
        void recalculateDetailsWidthRange();
        void schedulePreferencesSave();

        QString theme_{"light"};
        QString density_{"normal"};
        bool detailsVisible_{true};
        int detailsPanelWidth_{ports::kDefaultDetailsPanelWidth};
        int detailsViewportWidth_{1280};
        int detailsMinimumWidth_{340};
        int detailsPreferredWidth_{340};
        int detailsMaximumWidth_{380};
        static constexpr int kDetailsMinAbsolutePx = 340;
        static constexpr int kDetailsMaxAbsolutePx = 700;
        static constexpr int kDetailsMinRatioPercent = 30;
        static constexpr int kDetailsPrefRatioPercent = 45;
        static constexpr int kDetailsMaxRatioPercent = 50;
        QTimer preferencesSaveDebounce_;
    };

} // namespace ssa::presentation
