#pragma once

#include "domain/SsaTypes.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace ssa::presentation {

    class DetailsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString title READ title NOTIFY changed)
        Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)

      public:
        explicit DetailsViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString title() const;
        [[nodiscard]] QVariantList fields() const;
        void setRecord(const domain::SsaRecord* record);
        [[nodiscard]] QString selectedSsa() const;

      signals:
        void changed();

      private:
        QString title_;
        QVariantList fields_;
        QString selectedSsa_;
    };

} // namespace ssa::presentation
