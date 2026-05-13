#pragma once

#include "domain/SsaTypes.h"

#include <QCommandLineParser>

#include <string>
#include <vector>

namespace ssa::app::cli {

    class SsaCliRequestMapper final {
      public:
        [[nodiscard]] static std::vector<std::string>
        requestedColumns(const QCommandLineParser& parser);
        [[nodiscard]] static domain::SsaPageRequest
        pageRequest(const QCommandLineParser& parser,
                    const std::vector<std::string>& outputColumns);
    };

} // namespace ssa::app::cli
