#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace ssa::presentation {

    // ISO week stays separate from the live date/time shown in the status bar.
    class CurrentWeekViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString value READ value CONSTANT)
        Q_PROPERTY(QString label READ label CONSTANT)
        Q_PROPERTY(QString dateTimeLabel READ dateTimeLabel NOTIFY dateTimeLabelChanged)

      public:
        explicit CurrentWeekViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString value() const;
        [[nodiscard]] QString label() const;
        [[nodiscard]] QString dateTimeLabel() const;

      signals:
        void dateTimeLabelChanged();

      private:
        void refreshDateTimeLabel();

        QString value_;
        QString dateTimeLabel_;
        QTimer* timer_{nullptr};
    };

} // namespace ssa::presentation
