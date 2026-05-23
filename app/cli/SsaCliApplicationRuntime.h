#pragma once

#include "CliServiceFactory.h"

#include <QStringList>

namespace ssa::app::cli {

    class SsaCliApplicationRuntime final {
      public:
        [[nodiscard]] int run(const QStringList& arguments);

      private:
        CliServiceFactory serviceFactory_;
    };

} // namespace ssa::app::cli
