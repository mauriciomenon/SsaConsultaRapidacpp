#pragma once

#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ssa::presentation {

    class AdvancedSectorHierarchyViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList divisions READ divisions CONSTANT)
        Q_PROPERTY(QString selectedDivision READ selectedDivision NOTIFY changed)

      public:
        explicit AdvancedSectorHierarchyViewModel(filterpanel::FilterPanelAdvancedState& state,
                                                  QObject* parent = nullptr);

        [[nodiscard]] const QVariantList& divisions() const;
        [[nodiscard]] QString selectedDivision() const;

        Q_INVOKABLE void applyDivision(const QString& divisionKey);
        Q_INVOKABLE void clearDivision();
        void refreshFromState();

      signals:
        void changed();

      private:
        [[nodiscard]] QStringList executorIncludeValues() const;
        filterpanel::FilterPanelAdvancedState& state_;
        QVariantList divisions_;
    };

} // namespace ssa::presentation
