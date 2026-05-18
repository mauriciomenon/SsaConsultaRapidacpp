#pragma once

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class CurrentWeekViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString value READ value CONSTANT)
        Q_PROPERTY(QString label READ label CONSTANT)

      public:
        explicit CurrentWeekViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString value() const;
        [[nodiscard]] QString label() const;

      private:
        QString value_;
    };

} // namespace ssa::presentation
