#pragma once

#include "domain/SsaTypes.h"
#include "presentation/DetailsFieldsModel.h"

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class DetailsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString title READ title NOTIFY changed)
        Q_PROPERTY(DetailsFieldsModel* fields READ fields CONSTANT)
        Q_PROPERTY(int fieldCount READ fieldCount NOTIFY changed)

      public:
        explicit DetailsViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString title() const;
        [[nodiscard]] DetailsFieldsModel* fields();
        [[nodiscard]] int fieldCount() const;
        void setRecord(const domain::SsaRecord& record);
        void clearRecord();
        [[nodiscard]] QString selectedSsa() const;

      signals:
        void changed();

      private:
        QString title_;
        DetailsFieldsModel fields_;
        QString selectedSsa_;
    };

} // namespace ssa::presentation
