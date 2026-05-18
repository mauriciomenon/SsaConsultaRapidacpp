#include "presentation/FilterPanelAdvancedViewModel.h"

#include <utility>

namespace ssa::presentation {

    FilterPanelAdvancedViewModel::FilterPanelAdvancedViewModel(
        filterpanel::FilterPanelAdvancedState& state, QStringList weekColumnKeys, QObject* parent)
        : QObject(parent), state_(state), text_(state_, this),
          week_(state_, std::move(weekColumnKeys), this), derivation_(state_, this) {
        connect(&text_, &AdvancedTextFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
        connect(&week_, &AdvancedWeekFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
        connect(&derivation_, &AdvancedDerivationFilterViewModel::changed, this,
                &FilterPanelAdvancedViewModel::publishChanged);
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

    void FilterPanelAdvancedViewModel::clear() {
        state_.clear();
        text_.refreshFromState();
        publishChanged();
        emit applyRequested();
    }

    void FilterPanelAdvancedViewModel::refreshFromState() {
        text_.refreshFromState();
        publishChanged();
    }

    void FilterPanelAdvancedViewModel::publishChanged() {
        emit stateChanged();
        emit changed();
    }

} // namespace ssa::presentation
