#pragma once

#include <QObject>
#include <QString>

namespace ssa::platform {

    class SystemThemeResolver final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString currentTheme READ currentTheme NOTIFY systemThemeChanged)

      public:
        explicit SystemThemeResolver(QObject* parent = nullptr);
        ~SystemThemeResolver() override = default;

        [[nodiscard]] QString currentTheme() const;

      signals:
        void systemThemeChanged();

      private:
        void updateCurrentTheme();

        QString currentTheme_{"light"};
    };

} // namespace ssa::platform
