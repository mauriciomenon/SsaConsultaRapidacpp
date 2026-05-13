#pragma once

#include "ports/IExternalCommandPort.h"

#include <QUrl>

namespace ssa::platform {

    class SamCommandHandler final {
      public:
        explicit SamCommandHandler(QUrl samBaseUrl);

        [[nodiscard]] ports::ExternalCommandResult openHome() const;
        [[nodiscard]] ports::ExternalCommandResult openSsa(const std::string& ssaNumber) const;

      private:
        QUrl samBaseUrl_;
    };

} // namespace ssa::platform
