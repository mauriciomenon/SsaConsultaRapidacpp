#include "SsaCliRequestMapper.h"

#include "domain/SsaTypes.h"

#include <QStringList>

#include <stdexcept>

namespace ssa::app::cli {

    namespace {

        int optionInt(const QCommandLineParser& parser, const QString& name, const int fallback) {
            if (!parser.isSet(name)) {
                return fallback;
            }
            bool ok = false;
            const int value = parser.value(name).toInt(&ok);
            if (!ok) {
                throw std::invalid_argument("invalid integer option: " + name.toStdString());
            }
            return value;
        }

    } // namespace

    std::vector<std::string>
    SsaCliRequestMapper::requestedColumns(const QCommandLineParser& parser) {
        std::vector<std::string> columns;
        if (!parser.isSet("columns")) {
            return columns;
        }
        for (const auto& part : parser.value("columns").split(',', Qt::SkipEmptyParts)) {
            const auto key = part.trimmed().toStdString();
            if (!key.empty()) {
                columns.push_back(key);
            }
        }
        return columns;
    }

    domain::SsaPageRequest
    SsaCliRequestMapper::pageRequest(const QCommandLineParser& parser,
                                     const std::vector<std::string>& outputColumns) {
        domain::SsaPageRequest request;
        const int pageNumber = optionInt(parser, "page", 1);
        request.pageIndex = pageNumber <= 1 ? 0 : static_cast<std::size_t>(pageNumber - 1);
        request.pageSize =
            static_cast<std::size_t>(optionInt(parser, "page-size", domain::kDefaultPageSize));
        request.searchText = parser.value("search").toStdString();
        request.visibleColumns = outputColumns;
        request.excludeScaSesSte = !parser.isSet("include-sca-ses-ste");
        if (parser.isSet("asc") && parser.isSet("desc")) {
            throw std::invalid_argument("--asc and --desc cannot be used together");
        }
        if (parser.isSet("sort")) {
            request.sort.columnKey = parser.value("sort").toStdString();
            request.sort.ascending = !parser.isSet("desc");
        }
        return request;
    }

} // namespace ssa::app::cli
