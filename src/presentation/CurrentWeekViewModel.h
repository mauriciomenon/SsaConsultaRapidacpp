#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace ssa::presentation {

    // Live header label: ISO week + current date/time, refreshed each minute.
    class CurrentWeekViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString value READ value CONSTANT)
        Q_PROPERTY(QString label READ label CONSTANT)
        Q_PROPERTY(QString headerLabel READ headerLabel NOTIFY headerLabelChanged)

      public:
        explicit CurrentWeekViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString value() const;
        [[nodiscard]] QString label() const;
        [[nodiscard]] QString headerLabel() const;

      signals:
        void headerLabelChanged();

      private:
        void refreshHeaderLabel();

        QString value_;
        QString headerLabel_;
        QTimer* timer_{nullptr};
    };

} // namespace ssa::presentation
