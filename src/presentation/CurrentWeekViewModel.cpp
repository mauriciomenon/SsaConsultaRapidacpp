#include "presentation/CurrentWeekViewModel.h"

#include <QDate>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent) : QObject(parent) {
        int isoYear = 0;
        const int isoWeek = QDate::currentDate().weekNumber(&isoYear);
        value_ = QString("%1%2").arg(isoYear).arg(isoWeek, 2, 10, QLatin1Char('0'));
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::label() const {
        return QStringLiteral("Semana Atual: %1").arg(value_);
    }

} // namespace ssa::presentation
