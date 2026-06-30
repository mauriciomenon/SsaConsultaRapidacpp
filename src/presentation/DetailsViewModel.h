#pragma once

#include "domain/SsaTypes.h"
#include "presentation/DerivadasGraphModel.h"
#include "presentation/DetailsFieldsModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class DetailsViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString title READ title NOTIFY changed)
        Q_PROPERTY(DetailsFieldsModel* fields READ fields NOTIFY changed)
        Q_PROPERTY(QString selectedSsaNumber READ selectedSsaNumber NOTIFY changed)
        Q_PROPERTY(int fieldCount READ fieldCount NOTIFY changed)
        Q_PROPERTY(QVariantList relations READ relations NOTIFY changed)
        Q_PROPERTY(int relationCount READ relationCount NOTIFY changed)
        Q_PROPERTY(DerivadasGraphModel* graphModel READ graphModel CONSTANT)
        Q_PROPERTY(
            int currentRelationIndex READ currentRelationIndex NOTIFY relationNavigationChanged)
        Q_PROPERTY(
            bool canSelectNextRelation READ canSelectNextRelation NOTIFY relationNavigationChanged)
        Q_PROPERTY(bool canSelectPreviousRelation READ canSelectPreviousRelation NOTIFY
                       relationNavigationChanged)

      public:
        explicit DetailsViewModel(QObject* parent = nullptr);
        explicit DetailsViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                  QObject* parent = nullptr);

        [[nodiscard]] QString title() const;
        [[nodiscard]] DetailsFieldsModel* fields();
        [[nodiscard]] int fieldCount() const;
        void setRecord(const domain::SsaRecord& record);
        void clearRecord();
        // Loads a single SSA by number from the repository (if wired) and
        // displays it. Returns false if not found or no service attached.
        bool loadBySsaNumber(const QString& ssaNumber);
        [[nodiscard]] int currentRelationIndex() const;
        [[nodiscard]] bool canSelectNextRelation() const;
        [[nodiscard]] bool canSelectPreviousRelation() const;

      public slots:
        void selectNextRelation();
        void selectPreviousRelation();
        [[nodiscard]] QString selectedSsa() const;
        [[nodiscard]] QString selectedSsaNumber() const;
        [[nodiscard]] QVariantList relations() const;
        [[nodiscard]] int relationCount() const;
        [[nodiscard]] DerivadasGraphModel* graphModel();

      signals:
        void changed();
        void relationNavigationChanged();

      private:
        void rebuildDerivadas(const domain::SsaRecord& record);
        void setCurrentRelationIndex(int index);
        void loadRelation(int index);

        QString title_;
        DetailsFieldsModel fields_;
        QString selectedSsa_;
        QVariantList relations_;
        DerivadasGraphModel graphModel_;
        std::shared_ptr<query::SsaQueryService> queryService_;
        int currentRelationIndex_{0};
    };

} // namespace ssa::presentation
