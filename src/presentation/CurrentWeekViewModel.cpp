#include "presentation/CurrentWeekViewModel.h"

#include <QDateTime>
#include <QTimer>

#include <utility>

namespace ssa::presentation {

    CurrentWeekViewModel::CurrentWeekViewModel(QObject* parent)
        : CurrentWeekViewModel([] { return QDateTime::currentDateTime(); }, parent) {}

    CurrentWeekViewModel::CurrentWeekViewModel(CurrentDateTime currentDateTime, QObject* parent)
        : QObject(parent), currentDateTime_(std::move(currentDateTime)) {
        refresh();

        timer_ = new QTimer(this);
        timer_->setInterval(60 * 1000);
        connect(timer_, &QTimer::timeout, this, &CurrentWeekViewModel::refresh);
        timer_->start();
    }

    QString CurrentWeekViewModel::value() const {
        return value_;
    }

    QString CurrentWeekViewModel::dateTimeLabel() const {
        return dateTimeLabel_;
    }

    void CurrentWeekViewModel::refresh() {
        const auto now = currentDateTime_();
        int isoYear = 0;
        const int isoWeek = now.date().weekNumber(&isoYear);
        const auto newValue = QString::number(isoYear * 100 + isoWeek);
        if (newValue != value_) {
            value_ = newValue;
            emit valueChanged();
        }
        const auto newLabel = now.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        if (newLabel != dateTimeLabel_) {
            dateTimeLabel_ = newLabel;
            emit dateTimeLabelChanged();
        }
    }

} // namespace ssa::presentation
