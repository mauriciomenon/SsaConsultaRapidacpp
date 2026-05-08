#pragma once

#include "ports/IExternalCommandPort.h"

#include <QUrl>

namespace ssa::platform {

    class DesktopExternalCommandPort final : public ports::IExternalCommandPort {
      public:
        explicit DesktopExternalCommandPort(QUrl samBaseUrl = QUrl{"https://sam.itaipu.gov.br"});

        void openSamHome() override;
        void openSsa(const std::string& numeroSsa) override;
        void openPath(const std::string& path) override;
        void exportSelection(const std::vector<std::map<std::string, std::string>>& rows) override;
        void requestCommand(const std::string& command,
                            const std::map<std::string, std::string>& parameters) override;

      private:
        QUrl samBaseUrl_;
    };

} // namespace ssa::platform
