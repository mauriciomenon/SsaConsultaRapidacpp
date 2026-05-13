#include "presentation/UiSettingsViewModel.h"

namespace ssa::presentation {

    UiSettingsViewModel::UiSettingsViewModel(QObject* parent) : QObject(parent) {}

    QString UiSettingsViewModel::theme() const {
        return theme_;
    }

    void UiSettingsViewModel::setTheme(const QString& value) {
        if (theme_ == value) {
            return;
        }
        theme_ = value;
        emit preferencesSaveRequested();
        emit themeChanged();
        emit settingsChanged();
    }

    QString UiSettingsViewModel::density() const {
        return density_;
    }

    void UiSettingsViewModel::setDensity(const QString& value) {
        if (!isDensityValid(value) || density_ == value) {
            return;
        }
        density_ = value;
        emit preferencesSaveRequested();
        emit densityChanged();
        emit settingsChanged();
    }

    bool UiSettingsViewModel::detailsVisible() const {
        return detailsVisible_;
    }

    void UiSettingsViewModel::setDetailsVisible(const bool value) {
        if (detailsVisible_ == value) {
            return;
        }
        detailsVisible_ = value;
        emit preferencesSaveRequested();
        emit detailsVisibleChanged();
        emit settingsChanged();
    }

    int UiSettingsViewModel::detailsPanelWidth() const {
        return detailsPanelWidth_;
    }

    void UiSettingsViewModel::setDetailsPanelWidth(const int value) {
        const int bounded = ports::clampDetailsPanelWidth(value);
        if (detailsPanelWidth_ == bounded) {
            return;
        }
        detailsPanelWidth_ = bounded;
        emit preferencesSaveRequested();
        emit detailsPanelWidthChanged();
        emit settingsChanged();
    }

    void UiSettingsViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        theme_ = QString::fromStdString(snapshot.theme);
        const QString density = QString::fromStdString(snapshot.density);
        if (isDensityValid(density)) {
            density_ = density;
        }
        detailsVisible_ = snapshot.detailsVisible;
        detailsPanelWidth_ = ports::clampDetailsPanelWidth(snapshot.detailsPanelWidth);
        emit themeChanged();
        emit densityChanged();
        emit detailsVisibleChanged();
        emit detailsPanelWidthChanged();
        emit settingsChanged();
    }

    void UiSettingsViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        snapshot.theme = theme_.toStdString();
        snapshot.density = density_.toStdString();
        snapshot.detailsVisible = detailsVisible_;
        snapshot.detailsPanelWidth = detailsPanelWidth_;
    }

    bool UiSettingsViewModel::isDensityValid(const QString& value) const {
        return value == "compact" || value == "normal" || value == "comfortable";
    }

} // namespace ssa::presentation
