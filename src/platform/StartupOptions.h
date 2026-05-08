#pragma once

#include <QCommandLineParser>
#include <QString>

namespace ssa::platform {

    class StartupOptions final {
      public:
        [[nodiscard]] static StartupOptions fromParser(const QCommandLineParser& parser);

        QString projectRoot;
        QString databasePath;
        QString configDir;
    };

} // namespace ssa::platform
