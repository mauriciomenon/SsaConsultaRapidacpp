#pragma once

#include "domain/SsaTypes.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace ssa::app::cli {

    class SsaCliPrinter final {
      public:
        explicit SsaCliPrinter(std::ostream& output);

        void printPage(const domain::SsaPageResult& page,
                       const std::vector<std::string>& columns) const;
        void printDetails(const domain::SsaRecord& record) const;

      private:
        void printHeader(const std::vector<std::string>& columns) const;
        void printRow(const domain::SsaRecord& record,
                      const std::vector<std::string>& columns) const;

        std::ostream& output_;
    };

} // namespace ssa::app::cli
