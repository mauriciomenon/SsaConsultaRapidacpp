#include "presentation/AdvancedTextFilterRowModelFactory.h"

#include "domain/ColumnCatalog.h"

#include <QVariantMap>

#include <string>

namespace ssa::presentation {
    namespace {

        QVariantMap filterRow(const std::string_view key, const std::string_view label) {
            QVariantMap row;
            row.insert("key", QString::fromStdString(std::string{key}));
            row.insert("label", QString::fromStdString(std::string{label}));
            return row;
        }

    } // namespace

    QVariantList AdvancedTextFilterRowModelFactory::buildRows() const {
        QVariantList rows;
        for (const auto key : domain::ColumnCatalog::advancedFilterKeys()) {
            const auto* column = domain::ColumnCatalog::find(key);
            if (column != nullptr) {
                rows.push_back(filterRow(column->key,
                                         domain::ColumnCatalog::advancedFilterLabel(column->key)));
            }
        }
        return rows;
    }

} // namespace ssa::presentation
