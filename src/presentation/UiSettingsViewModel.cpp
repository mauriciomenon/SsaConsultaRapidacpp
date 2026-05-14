#include "presentation/UiSettingsViewModel.h"

#include <QGuiApplication>
#include <QStyleHints>

#include <algorithm>

namespace ssa::presentation {

    UiSettingsViewModel::UiSettingsViewModel(QObject* parent) : QObject(parent) {
        if (const auto hints = QGuiApplication::styleHints()) {
            connect(hints, &QStyleHints::colorSchemeChanged, this,
                    &UiSettingsViewModel::resolvedThemeChanged);
        }
        recalculateDetailsWidthRange();
        preferencesSaveDebounce_.setSingleShot(true);
        preferencesSaveDebounce_.setInterval(250);
        connect(&preferencesSaveDebounce_, &QTimer::timeout, this,
                &UiSettingsViewModel::preferencesSaveRequested);
    }

    QString UiSettingsViewModel::theme() const {
        return theme_;
    }

    void UiSettingsViewModel::setTheme(const QString& value) {
        if (theme_ == value) {
            return;
        }
        theme_ = value;
        emit themeChanged();
        emit resolvedThemeChanged();
        schedulePreferencesSave();
        emit settingsChanged();
    }

    QString UiSettingsViewModel::resolvedTheme() const {
        if (theme_ != "system") {
            return theme_;
        }
        const auto hints = QGuiApplication::styleHints();
        if (!hints) {
            return "light";
        }
        const auto scheme = hints->colorScheme();
        return scheme == Qt::ColorScheme::Dark ? "dark" : "light";
    }

    QString UiSettingsViewModel::density() const {
        return density_;
    }

    void UiSettingsViewModel::setDensity(const QString& value) {
        if (!isDensityValid(value) || density_ == value) {
            return;
        }
        density_ = value;
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
        const int bounded = ports::clampDetailsPanelWidth(value);
        if (detailsPanelWidth_ == bounded) {
            return;
        }
        detailsPanelWidth_ = bounded;
        emit detailsWidthLayoutChanged();
        emit detailsPanelWidthChanged();
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
        recalculateDetailsWidthRange();
        emit detailsWidthLayoutChanged();
    }

    int UiSettingsViewModel::detailsMinimumWidth() const {
        return detailsMinimumWidth_;
    }

    int UiSettingsViewModel::detailsPreferredWidth() const {
        return detailsPreferredWidth_;
    }

    int UiSettingsViewModel::detailsMaximumWidth() const {
        return detailsMaximumWidth_;
    }

    int UiSettingsViewModel::detailsEffectiveWidth() const {
        const int preferred = detailsPanelWidth_ > 0 ? detailsPanelWidth_ : detailsPreferredWidth_;
        return clampToWidthRange(preferred);
    }

    void UiSettingsViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        theme_ = QString::fromStdString(snapshot.theme);
        const QString density = QString::fromStdString(snapshot.density);
        if (isDensityValid(density)) {
            density_ = density;
        }
        detailsVisible_ = snapshot.detailsVisible;
        detailsPanelWidth_ = ports::clampDetailsPanelWidth(snapshot.detailsPanelWidth);
        recalculateDetailsWidthRange();
        emit themeChanged();
        emit resolvedThemeChanged();
        emit densityChanged();
        emit detailsVisibleChanged();
        emit detailsWidthLayoutChanged();
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

    int UiSettingsViewModel::computeDetailsMinimumWidth() const {
        const int widthByRatio = (detailsViewportWidth_ * kDetailsMinRatioPercent) / 100;
        return std::max(kDetailsMinAbsolutePx, widthByRatio);
    }

    int UiSettingsViewModel::computeDetailsMaximumWidth() const {
        const int widthByRatio = (detailsViewportWidth_ * kDetailsMaxRatioPercent) / 100;
        return std::max(computeDetailsMinimumWidth(),
                        std::min(kDetailsMaxAbsolutePx, widthByRatio));
    }

    int UiSettingsViewModel::computeDetailsPreferredWidth() const {
        const int widthByRatio = (detailsViewportWidth_ * kDetailsPrefRatioPercent) / 100;
        return std::max(kDetailsMinAbsolutePx,
                        std::min(computeDetailsMaximumWidth(), widthByRatio));
    }

    int UiSettingsViewModel::clampToWidthRange(const int value) const {
        return std::max(detailsMinimumWidth_, std::min(detailsMaximumWidth_, value));
    }

    void UiSettingsViewModel::recalculateDetailsWidthRange() {
        detailsMinimumWidth_ = computeDetailsMinimumWidth();
        detailsMaximumWidth_ = computeDetailsMaximumWidth();
        detailsPreferredWidth_ = computeDetailsPreferredWidth();
        emit detailsWidthLayoutChanged();
    }

    void UiSettingsViewModel::schedulePreferencesSave() {
        preferencesSaveDebounce_.start();
    }

} // namespace ssa::presentation
