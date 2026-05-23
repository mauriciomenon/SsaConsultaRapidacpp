#include "presentation/SsaRecordValueFormatter.h"

#include <QDate>
#include <QString>

namespace ssa::presentation {

    namespace {

        QString formattedDateText(const QString& text) {
            const QDate date = QDate::fromString(text.left(10), "yyyy-MM-dd");
            return date.isValid() ? date.toString("dd/MM/yyyy") : text;
        }

    } // namespace

    QVariant SsaRecordValueFormatter::valueFor(const domain::SsaRecord& record,
                                               const std::string& columnKey,
                                               const domain::ColumnType columnType) const {
        return valueFor(record.valueOf(columnKey), columnType);
    }

    QVariant SsaRecordValueFormatter::valueFor(const std::string_view value,
                                               const domain::ColumnType columnType) const {
        const QString text = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        switch (columnType) {
        case domain::ColumnType::Integer: {
            bool ok = false;
            const qlonglong number = text.toLongLong(&ok);
            if (ok) {
                return number;
            }
            return text;
        }
        case domain::ColumnType::DateText:
            return formattedDateText(text);
        case domain::ColumnType::Text:
            return text;
        }
        return text;
    }

} // namespace ssa::presentation
