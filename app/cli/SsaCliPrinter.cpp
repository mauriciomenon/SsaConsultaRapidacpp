#include "SsaCliPrinter.h"

#include <iostream>

namespace ssa::app::cli {

    SsaCliPrinter::SsaCliPrinter(std::ostream& output) : output_(output) {}

    void SsaCliPrinter::printPage(const domain::SsaPageResult& page,
                                  const std::vector<std::string>& columns) const {
        printHeader(columns);
        for (const auto& row : page.rows) {
            printRow(row, columns);
        }
        std::cerr << "rows=" << page.rows.size() << " total=" << page.totalRows
                  << " page=" << (page.totalRows == 0 ? 0 : page.pageIndex + 1) << '\n';
    }

    void SsaCliPrinter::printDetails(const domain::SsaRecord& record) const {
        for (const auto& [key, value] : record.fields()) {
            output_ << key << '=' << value << '\n';
        }
    }

    void SsaCliPrinter::printHeader(const std::vector<std::string>& columns) const {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            if (index > 0) {
                output_ << '\t';
            }
            output_ << columns[index];
        }
        output_ << '\n';
    }

    void SsaCliPrinter::printRow(const domain::SsaRecord& record,
                                 const std::vector<std::string>& columns) const {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            if (index > 0) {
                output_ << '\t';
            }
            // Control bytes would forge columns or lines in the tabular output.
            for (const char value : record.valueOf(columns[index])) {
                const auto byte = static_cast<unsigned char>(value);
                output_ << (byte < 0x20 || byte == 0x7f ? ' ' : value);
            }
        }
        output_ << '\n';
    }

} // namespace ssa::app::cli
