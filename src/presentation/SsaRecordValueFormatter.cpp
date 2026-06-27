#include "presentation/SsaRecordValueFormatter.h"

#include <QDate>
#include <QString>

#include <string>

namespace ssa::presentation {

    namespace {

        QString formattedDateText(const QString& text) {
            const QDate date = QDate::fromString(text.left(10), "yyyy-MM-dd");
            return date.isValid() ? date.toString("dd/MM/yyyy") : text;
        }

    } // namespace

    QVariant SsaRecordValueFormatter::valueFor(const domain::SsaRecord& record,
                                               const std::string& columnKey,
                                               const domain::ColumnType columnType) {
        return valueFor(record.valueOf(columnKey), columnType);
    }

    QVariant SsaRecordValueFormatter::valueFor(const std::string_view value,
                                               const domain::ColumnType columnType) {
        const QString text = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        switch (columnType) {
        case domain::ColumnType::Integer: {
            bool conversionOk = false;
            const qlonglong number = text.toLongLong(&conversionOk);
            if (conversionOk) {
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
