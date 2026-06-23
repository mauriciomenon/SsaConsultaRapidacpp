#include "presentation/FilterPanelAdvancedViewModel.h"

#include <utility>

namespace ssa::presentation {

    FilterPanelAdvancedViewModel::FilterPanelAdvancedViewModel(
        filterpanel::FilterPanelAdvancedState& state, filterpanel::FilterPanelState& filterState,
        QStringList weekColumnKeys, std::shared_ptr<query::SsaQueryService> queryService,
        QObject* parent)
        : QObject(parent), state_(state), text_(state_, this),
          week_(state_, std::move(weekColumnKeys), this), derivation_(state_, this),
          sectorHierarchy_(state_, this),
          macro_(state_, filterState, std::move(queryService), this) {
        connect(&text_, &AdvancedTextFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
        connect(&week_, &AdvancedWeekFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
        connect(&derivation_, &AdvancedDerivationFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
        connect(&sectorHierarchy_, &AdvancedSectorHierarchyViewModel::changed, this, [this] {
            text_.refreshFromState();
            publishChanged();
        });
        connect(&macro_, &AdvancedMacroFilterViewModel::changed, this, [this] {
            text_.refreshFromState();
            publishChanged();
        });
    }

    QObject* FilterPanelAdvancedViewModel::text() {
        return &text_;
    }

    QObject* FilterPanelAdvancedViewModel::week() {
        return &week_;
    }

    QObject* FilterPanelAdvancedViewModel::derivation() {
        return &derivation_;
    }

    QObject* FilterPanelAdvancedViewModel::sectorHierarchy() {
        return &sectorHierarchy_;
    }

    QObject* FilterPanelAdvancedViewModel::macro() {
        return &macro_;
    }

    void FilterPanelAdvancedViewModel::clear() {
        state_.clear();
        text_.refreshFromState();
        week_.refreshFromState();
        derivation_.refreshFromState();
        sectorHierarchy_.refreshFromState();
        macro_.refreshFromState();
        publishChanged();
        emit applyRequested();
    }

    void FilterPanelAdvancedViewModel::refreshFromState() {
        text_.refreshFromState();
        week_.refreshFromState();
        derivation_.refreshFromState();
        sectorHierarchy_.refreshFromState();
        macro_.refreshFromState();
        publishChanged();
    }

    void FilterPanelAdvancedViewModel::publishChanged() {
        emit stateChanged();
        emit changed();
    }

} // namespace ssa::presentation
