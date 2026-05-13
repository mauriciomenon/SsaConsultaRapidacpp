#include "presentation/SsaRecordValueFormatter.h"

#include <QString>

namespace ssa::presentation {

    QVariant SsaRecordValueFormatter::valueFor(const domain::SsaRecord& record,
                                               const std::string& columnKey,
                                               const domain::ColumnType columnType) const {
        return valueFor(record.valueOf(columnKey), columnType);
    }

    QVariant SsaRecordValueFormatter::valueFor(const std::string_view value,
                                               const domain::ColumnType columnType) const {
        const QString text = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        if (columnType == domain::ColumnType::Integer) {
            bool ok = false;
            const qlonglong number = text.toLongLong(&ok);
            if (ok) {
                return number;
            }
        }
        return text;
    }

} // namespace ssa::presentation
