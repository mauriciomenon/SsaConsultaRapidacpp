#include "presentation/UiSettingsViewModel.h"

namespace ssa::presentation {

    UiSettingsViewModel::UiSettingsViewModel(QObject* parent) : QObject(parent) {
        emitDetailsWidthLayoutChanged();
        preferencesSaveDebounce_.setSingleShot(true);
        preferencesSaveDebounce_.setInterval(250);
        connect(&preferencesSaveDebounce_, &QTimer::timeout, this,
                &UiSettingsViewModel::preferencesSaveRequested);
    }

    QString UiSettingsViewModel::theme() const {
        return theme_;
    }

    void UiSettingsViewModel::setTheme(const QString& value) {
        if (!applyThemeValue(value)) {
            return;
        }
        emit themeChanged();
        emit resolvedThemeChanged();
        schedulePreferencesSave();
        emit settingsChanged();
    }

    QString UiSettingsViewModel::resolvedTheme() const {
        if (theme_ != "system") {
            return theme_;
        }
        return systemTheme_;
    }

    void UiSettingsViewModel::setSystemTheme(const QString& theme) {
        if (systemTheme_ == theme) {
            return;
        }
        systemTheme_ = theme;
        if (theme_ == "system") {
            emit resolvedThemeChanged();
        }
    }

    QString UiSettingsViewModel::density() const {
        return density_;
    }

    void UiSettingsViewModel::setDensity(const QString& value) {
        if (!applyDensityValue(value)) {
            return;
        }
        emit densityChanged();
        schedulePreferencesSave();
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
        emit detailsVisibleChanged();
        schedulePreferencesSave();
        emit settingsChanged();
    }

    int UiSettingsViewModel::detailsPanelWidth() const {
        return detailsPanelWidth_;
    }

    void UiSettingsViewModel::setDetailsPanelWidth(const int value) {
        if (!applyDetailsPanelWidthValue(value)) {
            return;
        }
        emit detailsPanelWidthChanged();
        emitDetailsWidthLayoutChanged();
        schedulePreferencesSave();
        emit settingsChanged();
    }

    int UiSettingsViewModel::detailsViewportWidth() const {
        return detailsViewportWidth_;
    }

    void UiSettingsViewModel::setDetailsViewportWidth(const int value) {
        const int normalizedWidth = value > 0 ? value : 1;
        if (detailsViewportWidth_ == normalizedWidth) {
            return;
        }
        detailsViewportWidth_ = normalizedWidth;
        emitDetailsWidthLayoutChanged();
    }

    int UiSettingsViewModel::detailsMinimumWidth() const {
        return detailsLayoutGeometry().minimumWidth;
    }

    int UiSettingsViewModel::detailsPreferredWidth() const {
        return detailsLayoutGeometry().preferredWidth;
    }

    int UiSettingsViewModel::detailsMaximumWidth() const {
        return detailsLayoutGeometry().maximumWidth;
    }

    void UiSettingsViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        const QString theme = QString::fromStdString(snapshot.theme);
        const bool themeWasUpdated = applyThemeValue(theme);
        const QString density = QString::fromStdString(snapshot.density);
        const bool densityWasUpdated = applyDensityValue(density);
        detailsVisible_ = snapshot.detailsVisible;
        applyDetailsPanelWidthValue(snapshot.detailsPanelWidth);
        emitDetailsWidthLayoutChanged();
        if (themeWasUpdated) {
            emit themeChanged();
            emit resolvedThemeChanged();
        }
        if (densityWasUpdated) {
            emit densityChanged();
        }
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

    details_layout::DetailsPanelGeometry UiSettingsViewModel::detailsLayoutGeometry() const {
        return details_layout::computeDetailsPanelGeometry(detailsViewportWidth_);
    }

    void UiSettingsViewModel::emitDetailsWidthLayoutChanged() {
        emit detailsWidthLayoutChanged();
    }

    bool UiSettingsViewModel::applyThemeValue(const QString& value) {
        if (!ports::isThemeValid(value.toStdString()) || theme_ == value) {
            return false;
        }
        theme_ = value;
        return true;
    }

    bool UiSettingsViewModel::applyDensityValue(const QString& value) {
        if (!ports::isDensityValid(value.toStdString()) || density_ == value) {
            return false;
        }
        density_ = value;
        return true;
    }

    bool UiSettingsViewModel::applyDetailsPanelWidthValue(const int value) {
        const int bounded = ports::clampDetailsPanelWidth(value);
        if (detailsPanelWidth_ == bounded) {
            return false;
        }
        detailsPanelWidth_ = bounded;
        return true;
    }

    void UiSettingsViewModel::schedulePreferencesSave() {
        preferencesSaveDebounce_.start();
    }

} // namespace ssa::presentation
