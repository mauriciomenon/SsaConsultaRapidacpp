#pragma once

#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedMacroFilterViewModel.h"
#include "presentation/AdvancedSectorHierarchyViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class FilterPanelAdvancedViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QObject* text READ text CONSTANT)
        Q_PROPERTY(QObject* week READ week CONSTANT)
        Q_PROPERTY(QObject* derivation READ derivation CONSTANT)
        Q_PROPERTY(QObject* sectorHierarchy READ sectorHierarchy CONSTANT)
        Q_PROPERTY(QObject* macro READ macro CONSTANT)

      public:
        FilterPanelAdvancedViewModel(filterpanel::FilterPanelAdvancedState& state,
                                     filterpanel::FilterPanelState& filterState,
                                     QStringList weekColumnKeys,
                                     std::shared_ptr<query::SsaQueryService> queryService,
                                     QObject* parent = nullptr);

        [[nodiscard]] QObject* text();
        [[nodiscard]] QObject* week();
        [[nodiscard]] QObject* derivation();
        [[nodiscard]] QObject* sectorHierarchy();
        [[nodiscard]] QObject* macro();
        void setQuickSector(const QString& value);
        void refreshFromState();

      public slots:
        void clear();

      signals:
        void changed();
        void stateChanged();
        void applyRequested();
        void textFilterApplied(QString key, QString expression);

      private:
        void publishChanged();

        filterpanel::FilterPanelAdvancedState& state_;
        AdvancedTextFilterViewModel text_;
        AdvancedWeekFilterViewModel week_;
        AdvancedDerivationFilterViewModel derivation_;
        AdvancedSectorHierarchyViewModel sectorHierarchy_;
        AdvancedMacroFilterViewModel macro_;
    };

} // namespace ssa::presentation
