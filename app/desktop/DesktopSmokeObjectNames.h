#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace ssa::app::desktop::smoke_object_names {

    inline constexpr auto preferencesDialog = "ssaSmokePreferencesDialog";

} // namespace ssa::app::desktop::smoke_object_names

namespace ssa::app::desktop {

    class DesktopSmokeObjectNames final : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON
        Q_PROPERTY(QString preferencesDialog READ preferencesDialog CONSTANT)

      public:
        [[nodiscard]] QString preferencesDialog() const {
            return QString::fromLatin1(smoke_object_names::preferencesDialog);
        }
    };

} // namespace ssa::app::desktop
