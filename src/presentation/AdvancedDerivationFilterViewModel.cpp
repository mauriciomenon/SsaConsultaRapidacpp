#include "presentation/AdvancedDerivationFilterViewModel.h"

namespace ssa::presentation {
    namespace {
        QString normalizedValues(const QStringList& values) {
            QStringList normalized;
            for (const auto& value : values) {
                const auto trimmed = value.trimmed();
                if (!trimmed.isEmpty() && !normalized.contains(trimmed)) {
                    normalized.push_back(trimmed);
                }
            }
            normalized.sort();
            return normalized.join(QStringLiteral(","));
        }
    } // namespace

    AdvancedDerivationFilterViewModel::AdvancedDerivationFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state) {}

    QString AdvancedDerivationFilterViewModel::reprogrammingMode() const {
        return state_.reprogrammingMode();
    }

    void AdvancedDerivationFilterViewModel::setReprogrammingMode(const QString& value) {
        if (!state_.setReprogrammingMode(value)) {
            return;
        }
        emit changed();
    }

    QStringList AdvancedDerivationFilterViewModel::reprogrammingModeOptions() const {
        return reprogrammingModeOptions_;
    }

    QStringList AdvancedDerivationFilterViewModel::reprogrammingValues() const {
        return state_.reprogrammingValues().split(QStringLiteral(","), Qt::SkipEmptyParts);
    }

    void AdvancedDerivationFilterViewModel::setReprogrammingValues(const QStringList& values) {
        if (!state_.setReprogrammingValues(normalizedValues(values))) {
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

    void AdvancedDerivationFilterViewModel::refreshFromState() {
        emit changed();
    }

} // namespace ssa::presentation
