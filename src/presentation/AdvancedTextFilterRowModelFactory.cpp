#include "presentation/AdvancedTextFilterRowModelFactory.h"

#include "domain/ColumnCatalog.h"
#include "presentation/SsaColumnDisplayCatalog.h"

#include <QVariantMap>

#include <string>

namespace ssa::presentation {
    namespace {

        QVariantMap filterRow(const std::string_view key, const std::string_view label,
                              const std::string_view shortLabel) {
            QVariantMap row;
            row.insert("key", QString::fromStdString(std::string{key}));
            row.insert("label", QString::fromStdString(std::string{label}));
            row.insert("labelShort", QString::fromStdString(std::string{shortLabel}));
            return row;
        }

    } // namespace

    QVariantList AdvancedTextFilterRowModelFactory::buildRows() const {
        QVariantList rows;
        const SsaColumnDisplayCatalog displayCatalog;
        for (const auto key : domain::ColumnCatalog::advancedFilterKeys()) {
            rows.push_back(filterRow(key, displayCatalog.advancedFilterLabel(key),
                                     displayCatalog.advancedFilterShortLabel(key)));
        }
        return rows;
    }

} // namespace ssa::presentation
