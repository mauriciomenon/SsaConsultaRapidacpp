#include "presentation/CurrentWeekViewModel.h"

#include <QDate>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent) : QObject(parent) {
        int isoYear = 0;
        const int isoWeek = QDate::currentDate().weekNumber(&isoYear);
        value_ = QString::number(isoYear * 100 + isoWeek);
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::label() const {
        return value_;
    }

} // namespace ssa::presentation
