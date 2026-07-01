#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace ssa::app::desktop::smoke_object_names {

    inline constexpr auto preferencesDialog = "ssaSmokePreferencesDialog";
    inline constexpr auto themeDialog = "ssaSmokeThemeDialog";
    inline constexpr auto detailsWindow = "ssaSmokeDetailsWindow";

} // namespace ssa::app::desktop::smoke_object_names

namespace ssa::app::desktop {

    class DesktopSmokeObjectNames final : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON
        Q_PROPERTY(QString preferencesDialog READ preferencesDialog CONSTANT)
        Q_PROPERTY(QString themeDialog READ themeDialog CONSTANT)
        Q_PROPERTY(QString detailsWindow READ detailsWindow CONSTANT)

      public:
        [[nodiscard]] QString preferencesDialog() const {
            return QString::fromLatin1(smoke_object_names::preferencesDialog);
        }
        [[nodiscard]] QString themeDialog() const {
            return QString::fromLatin1(smoke_object_names::themeDialog);
        }
        [[nodiscard]] QString detailsWindow() const {
            return QString::fromLatin1(smoke_object_names::detailsWindow);
        }
    };

} // namespace ssa::app::desktop
