#include "presentation/AdvancedDerivationFilterViewModel.h"

namespace ssa::presentation {

    AdvancedDerivationFilterViewModel::AdvancedDerivationFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state) {}

    QString AdvancedDerivationFilterViewModel::reprogrammingEqualsFilter() const {
        return state_.reprogrammingEquals();
    }

    void AdvancedDerivationFilterViewModel::setReprogrammingEqualsFilter(const QString& value) {
        if (!state_.setReprogrammingEquals(value)) {
            return;
        }
        emit changed();
    }

    QString AdvancedDerivationFilterViewModel::derivationMode() const {
        return state_.derivationMode();
    }

    void AdvancedDerivationFilterViewModel::setDerivationMode(const QString& value) {
        if (!state_.setDerivationMode(value)) {
            return;
        }
        emit changed();
    }

    QStringList AdvancedDerivationFilterViewModel::derivationModeOptions() const {
        return derivationModeOptions_;
    }

    bool AdvancedDerivationFilterViewModel::onlyReprogrammed() const {
        return state_.onlyReprogrammed();
    }

    void AdvancedDerivationFilterViewModel::setOnlyReprogrammed(bool value) {
        if (!state_.setOnlyReprogrammed(value)) {
            return;
        }
        emit changed();
    }

} // namespace ssa::presentation
