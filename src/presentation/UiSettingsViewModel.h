#pragma once

#include "ports/IUserPreferencesStore.h"
#include "ports/UserPreferenceDefaults.h"

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class UiSettingsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
        Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY densityChanged)
        Q_PROPERTY(bool detailsVisible READ detailsVisible WRITE setDetailsVisible NOTIFY
                       detailsVisibleChanged)
        Q_PROPERTY(int detailsPanelWidth READ detailsPanelWidth WRITE setDetailsPanelWidth NOTIFY
                       detailsPanelWidthChanged)

      public:
        explicit UiSettingsViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString theme() const;
        void setTheme(const QString& value);
        [[nodiscard]] QString density() const;
        void setDensity(const QString& value);
        [[nodiscard]] bool detailsVisible() const;
        void setDetailsVisible(bool value);
        [[nodiscard]] int detailsPanelWidth() const;
        void setDetailsPanelWidth(int value);

        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

      signals:
        void themeChanged();
        void densityChanged();
        void detailsVisibleChanged();
        void detailsPanelWidthChanged();
        void settingsChanged();
        void preferencesSaveRequested();

      private:
        [[nodiscard]] bool isDensityValid(const QString& value) const;

        QString theme_{"system"};
        QString density_{"normal"};
        bool detailsVisible_{true};
        int detailsPanelWidth_{ports::kDefaultDetailsPanelWidth};
    };

} // namespace ssa::presentation
