#include "presentation/CurrentWeekViewModel.h"

#include <QDate>
#include <QDateTime>
#include <QTimer>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent) : QObject(parent) {
        int isoYear = 0;
        const int isoWeek = QDate::currentDate().weekNumber(&isoYear);
        value_ = QString::number(isoYear * 100 + isoWeek);
        refreshDateTimeLabel();

        timer_ = new QTimer(this);
        timer_->setInterval(60 * 1000);
        connect(timer_, &QTimer::timeout, this, &CurrentWeekViewModel::refreshDateTimeLabel);
        timer_->start();
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::label() const {
        return value_;
    }

    QString CurrentWeekViewModel::dateTimeLabel() const {
        return dateTimeLabel_;
    }

    void CurrentWeekViewModel::refreshDateTimeLabel() {
        const auto now = QDateTime::currentDateTime();
        const auto newLabel = now.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        if (newLabel != dateTimeLabel_) {
            dateTimeLabel_ = newLabel;
            emit dateTimeLabelChanged();
        }
    }

} // namespace ssa::presentation
