#include "presentation/AdvancedTextFilterViewModel.h"

#include "presentation/AdvancedTextFilterRowModelFactory.h"

namespace ssa::presentation {

    AdvancedTextFilterViewModel::AdvancedTextFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state), rows_(AdvancedTextFilterRowModelFactory{}.buildRows()),
          operatorModes_{{QVariantMap{{"label", tr("Igual")}, {"mode", "equals"}}},
                         {QVariantMap{{"label", tr("Diferente")}, {"mode", "different"}}}},
          operatorModeIndex_{{"equals", 0}, {"different", 1}} {
        refreshFromState();
    }

    const QVariantList& AdvancedTextFilterViewModel::rows() const {
        return rows_;
    }

    const QVariantList& AdvancedTextFilterViewModel::operatorModes() const {
        return operatorModes_;
    }

    int AdvancedTextFilterViewModel::version() const {
        return version_;
    }

    QString AdvancedTextFilterViewModel::operatorModeFor(const QString& key) const {
        return columns_.operatorModeFor(key);
    }

    QString AdvancedTextFilterViewModel::operatorLabelFor(const QString& key) const {
        const auto mode = operatorModeFor(key);
        const auto index = operatorModeIndex_.constFind(mode);
        if (index == operatorModeIndex_.constEnd()) {
            return mode == "mixed" ? tr("Misto") : QString{};
        }
        if (index.value() < 0 || index.value() >= operatorModes_.size()) {
            return {};
        }
        return operatorModes_.at(index.value()).toMap().value("label").toString();
    }

    int AdvancedTextFilterViewModel::operatorIndexFor(const QString& key) const {
        const auto it = operatorModeIndex_.constFind(operatorModeFor(key));
        return it == operatorModeIndex_.constEnd() ? -1 : it.value();
    }

    void AdvancedTextFilterViewModel::setOperatorMode(const QString& key,
                                                      const QString& operatorMode) {
        const auto currentExpression = textFilter(key);
        if (!columns_.setOperatorMode(key, operatorMode, currentExpression)) {
            return;
        }
        publishChanged();
    }

    QString AdvancedTextFilterViewModel::textFilter(const QString& key) const {
        return state_.textFilter(key);
    }

    void AdvancedTextFilterViewModel::setTextFilter(const QString& key, const QString& value) {
        if (!state_.setTextFilter(key, value)) {
            return;
        }
        columns_.setExpression(key, value);
        publishChanged();
    }

    bool AdvancedTextFilterViewModel::addSelectedValue(const QString& key, const QString& value) {
        const auto expression =
            columns_.expressionWithAddedValue(key, textFilter(key), value, operatorModeFor(key));
        if (!expression.has_value()) {
            return false;
        }
        return publishExpression(key, *expression, true);
    }

    void AdvancedTextFilterViewModel::replaceWithOperatorValueList(const QString& key,
                                                                   const QStringList& values,
                                                                   const QString& operatorMode) {
        columns_.setOperatorMode(key, operatorMode, textFilter(key));
        publishExpression(key, columns_.expressionReplacingWithOperator(values, operatorMode),
                          true);
    }

    void AdvancedTextFilterViewModel::refreshFromState() {
        if (columns_.refreshFrom(state_.textFilters())) {
            publishChanged();
        }
    }

    bool AdvancedTextFilterViewModel::publishExpression(const QString& key,
                                                        const QString& expression,
                                                        const bool inferOperatorMode) {
        if (!state_.setTextFilter(key, expression)) {
            return false;
        }
        columns_.setExpression(key, expression, inferOperatorMode);
        publishChanged();
        return true;
    }

    void AdvancedTextFilterViewModel::publishChanged() {
        ++version_;
        emit changed();
    }

} // namespace ssa::presentation
