#pragma once

#include <QCommandLineParser>
#include <QString>

namespace ssa::platform {

    class StartupOptions final {
      public:
        [[nodiscard]] static StartupOptions fromParser(const QCommandLineParser& parser);
        [[nodiscard]] static QString defaultSamBaseUrl();

        QString projectRoot;
        QString databasePath;
        QString configDir;
        QString samBaseUrl;
    };

} // namespace ssa::platform
