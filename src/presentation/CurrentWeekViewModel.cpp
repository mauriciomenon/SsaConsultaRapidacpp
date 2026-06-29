#include "presentation/CurrentWeekViewModel.h"

#include <QDate>
#include <QDateTime>
#include <QTimer>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent) : QObject(parent) {
        int isoYear = 0;
        const int isoWeek = QDate::currentDate().weekNumber(&isoYear);
        value_ = QString::number(isoYear * 100 + isoWeek);
        refreshHeaderLabel();

        timer_ = new QTimer(this);
        timer_->setInterval(60 * 1000);
        connect(timer_, &QTimer::timeout, this, &CurrentWeekViewModel::refreshHeaderLabel);
        timer_->start();
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::label() const {
        return value_;
    }

    QString CurrentWeekViewModel::headerLabel() const {
        return headerLabel_;
    }

    void CurrentWeekViewModel::refreshHeaderLabel() {
        const auto now = QDateTime::currentDateTime();
        const auto newLabel =
            QStringLiteral("%1 - %2").arg(now.toString(QStringLiteral("dd/MM/yyyy HH:mm")), value_);
        if (newLabel != headerLabel_) {
            headerLabel_ = newLabel;
            emit headerLabelChanged();
        }
    }

} // namespace ssa::presentation
