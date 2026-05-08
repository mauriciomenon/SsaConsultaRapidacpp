#pragma once

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class StatusViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString message READ message NOTIFY changed)
        Q_PROPERTY(QString error READ error NOTIFY changed)
        Q_PROPERTY(bool loading READ loading NOTIFY changed)

      public:
        explicit StatusViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString message() const;
        [[nodiscard]] QString error() const;
        [[nodiscard]] bool loading() const;
        void setMessage(const QString& value);
        void setError(const QString& value);
        void setLoading(bool value);

      signals:
        void changed();

      private:
        QString message_{"Pronto"};
        QString error_;
        bool loading_{false};
    };

} // namespace ssa::presentation
