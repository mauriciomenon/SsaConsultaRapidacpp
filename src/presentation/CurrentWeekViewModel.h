#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace ssa::presentation {

    // ISO week stays separate from the live date/time shown in the status bar.
    class CurrentWeekViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString value READ value NOTIFY valueChanged)
        Q_PROPERTY(QString dateTimeLabel READ dateTimeLabel NOTIFY dateTimeLabelChanged)

      public:
        using CurrentDateTime = std::function<QDateTime()>;

        explicit CurrentWeekViewModel(QObject* parent = nullptr);
        explicit CurrentWeekViewModel(CurrentDateTime currentDateTime, QObject* parent = nullptr);

        [[nodiscard]] QString value() const;
        [[nodiscard]] QString dateTimeLabel() const;

      signals:
        void valueChanged();
        void dateTimeLabelChanged();

      private slots:
        void refresh();

      private:
        CurrentDateTime currentDateTime_;
        QString value_;
        QString dateTimeLabel_;
        QTimer* timer_{nullptr};
    };

} // namespace ssa::presentation
