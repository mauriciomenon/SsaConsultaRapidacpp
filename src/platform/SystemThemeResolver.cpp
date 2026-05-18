#include "platform/SystemThemeResolver.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace ssa::platform {

    SystemThemeResolver::SystemThemeResolver(QObject* parent) : QObject(parent) {
        if (const auto hints = QGuiApplication::styleHints()) {
            connect(hints, &QStyleHints::colorSchemeChanged, this,
                    [this] { updateCurrentTheme(); });
            updateCurrentTheme();
        }
    }

    QString SystemThemeResolver::currentTheme() const {
        return currentTheme_;
    }

    void SystemThemeResolver::updateCurrentTheme() {
        const auto hints = QGuiApplication::styleHints();
        const auto scheme = hints ? hints->colorScheme() : Qt::ColorScheme::Light;
        const QString resolved = scheme == Qt::ColorScheme::Dark ? "dark" : "light";
        if (currentTheme_ == resolved) {
            return;
        }
        currentTheme_ = resolved;
        emit systemThemeChanged();
    }

} // namespace ssa::platform
