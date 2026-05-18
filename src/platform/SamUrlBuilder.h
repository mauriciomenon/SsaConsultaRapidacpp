#pragma once

#include <QUrl>

#include <string>

namespace ssa::platform {

    class SamUrlBuilder final {
      public:
        [[nodiscard]] static QUrl publicSsaUrl(const QUrl& baseUrl, const std::string& ssaNumber);
    };

} // namespace ssa::platform
