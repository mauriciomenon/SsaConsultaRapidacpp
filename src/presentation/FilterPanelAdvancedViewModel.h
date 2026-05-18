#pragma once

#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ssa::presentation {

    class FilterPanelAdvancedViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QObject* text READ text CONSTANT)
        Q_PROPERTY(QObject* week READ week CONSTANT)
        Q_PROPERTY(QObject* derivation READ derivation CONSTANT)

      public:
        FilterPanelAdvancedViewModel(filterpanel::FilterPanelAdvancedState& state,
                                     QStringList weekColumnKeys, QObject* parent = nullptr);

        [[nodiscard]] QObject* text();
        [[nodiscard]] QObject* week();
        [[nodiscard]] QObject* derivation();
        void refreshFromState();

      public slots:
        void clear();

      signals:
        void changed();
        void stateChanged();
        void applyRequested();

      private:
        void publishChanged();

        filterpanel::FilterPanelAdvancedState& state_;
        AdvancedTextFilterViewModel text_;
        AdvancedWeekFilterViewModel week_;
        AdvancedDerivationFilterViewModel derivation_;
    };

} // namespace ssa::presentation
