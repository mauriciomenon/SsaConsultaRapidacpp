#include "presentation/CurrentWeekViewModel.h"

#include <QDate>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent) : QObject(parent) {
        const int isoWeek = QDate::currentDate().weekNumber(nullptr);
        value_ = QString::number(isoWeek);
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::label() const {
        return value_;
    }

} // namespace ssa::presentation
