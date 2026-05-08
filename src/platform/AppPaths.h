#pragma once

#include <QString>

namespace ssa::platform {

    class AppPaths final {
      public:
        AppPaths(QString projectRoot, QString configDir);

        [[nodiscard]] QString projectRoot() const;
        [[nodiscard]] QString configDir() const;
        [[nodiscard]] QString preferencesFile() const;

      private:
        QString projectRoot_;
        QString configDir_;
    };

} // namespace ssa::platform
