#pragma once

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class SearchViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

      public:
        explicit SearchViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString text() const;
        void setText(const QString& value);

      signals:
        void textChanged();
        void applyRequested();
        void textClearRequested();

      public slots:
        void apply();
        void clear();

      private:
        QString text_;
    };

} // namespace ssa::presentation
