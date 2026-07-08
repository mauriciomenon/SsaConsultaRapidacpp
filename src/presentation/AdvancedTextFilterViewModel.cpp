#include "presentation/AdvancedTextFilterViewModel.h"

#include "domain/ColumnCatalog.h"
#include "presentation/AdvancedTextFilterRowModelFactory.h"
#include "presentation/FilterPanelStateHelpers.h"

#include <utility>

namespace ssa::presentation {

    namespace {

        QString executorColumnKey() {
            const auto key = domain::ColumnCatalog::executorColumnKey();
            return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
        }

    } // namespace

    AdvancedTextFilterViewModel::AdvancedTextFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state), rows_(AdvancedTextFilterRowModelFactory{}.buildRows()),
          operatorModes_{{QVariantMap{{"label", QStringLiteral("=")}, {"mode", "equals"}}},
                         {QVariantMap{{"label", QStringLiteral("\u2260")}, {"mode", "different"}}}},
          operatorModeIndex_{{"equals", 0}, {"different", 1}} {
        refreshFromState();
        if (cardStates_.empty()) {
            rebuildCardStates();
        }
    }

    const QVariantList& AdvancedTextFilterViewModel::rows() const {
        return rows_;
    }

    const QVariantList& AdvancedTextFilterViewModel::cardStates() const {
        return cardStates_;
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
        const auto modeEntry = operatorModeIndex_.constFind(operatorModeFor(key));
        return modeEntry == operatorModeIndex_.constEnd() ? -1 : modeEntry.value();
    }

    void AdvancedTextFilterViewModel::setOperatorMode(const QString& key,
                                                      const QString& operatorMode) {
        const auto currentExpression = effectiveTextFilter(key);
        if (!columns_.setOperatorMode({.key = key,
                                       .operatorMode = operatorMode,
                                       .currentExpression = currentExpression})) {
            return;
        }
        publishChangedFor(key);
    }

    QString AdvancedTextFilterViewModel::textFilter(const QString& key) const {
        return state_.textFilter(key);
    }

    void AdvancedTextFilterViewModel::setTextFilter(const QString& key, const QString& value) {
        if (!state_.setTextFilter(key, value)) {
            return;
        }
        columns_.setExpression({.key = key, .expression = value});
        publishChangedFor(key);
    }

    bool AdvancedTextFilterViewModel::clearTextFilterAndApply(const QString& key) {
        if (textFilter(key).trimmed().isEmpty() && !effectiveTextFilter(key).trimmed().isEmpty()) {
            emit expressionApplied(key, {});
            emit applyRequested();
            return true;
        }
        return publishExpressionAndApply(key, {}, true);
    }

    bool AdvancedTextFilterViewModel::updateFilterWithSelectedValue(const QString& key,
                                                                    const QString& value) {
        const auto expression =
            columns_.expressionWithAddedValue({.key = key,
                                               .currentExpression = effectiveTextFilter(key),
                                               .value = value,
                                               .operatorMode = operatorModeFor(key)});
        if (!expression.has_value()) {
            return false;
        }
        return publishExpressionAndApply(key, *expression, true);
    }

    void AdvancedTextFilterViewModel::replaceWithOperatorValueList(const QString& key,
                                                                   const QStringList& values,
                                                                   const QString& operatorMode) {
        columns_.setOperatorMode({.key = key,
                                  .operatorMode = operatorMode,
                                  .currentExpression = effectiveTextFilter(key)});
        publishExpressionAndApply(key,
                                  AdvancedTextFilterColumnStore::expressionReplacingWithOperator(
                                      {.values = values, .operatorMode = operatorMode}),
                                  false);
    }

    void AdvancedTextFilterViewModel::replaceWithOperatorValueLists(
        const QString& key, const QStringList& includeValues, const QStringList& excludeValues) {
        publishExpressionAndApply(
            key,
            AdvancedTextFilterColumnStore::expressionReplacingWithOperatorLists(
                {.includeValues = includeValues, .excludeValues = excludeValues}),
            true);
    }

    void AdvancedTextFilterViewModel::setQuickSector(QString value) {
        value = value.trimmed();
        if (quickSector_ == value) {
            return;
        }
        quickSector_ = std::move(value);
        publishChangedFor(executorColumnKey());
    }

    void AdvancedTextFilterViewModel::refreshFromState() {
        if (columns_.refreshFrom(state_.textFilters())) {
            publishChanged();
        }
    }

    QString AdvancedTextFilterViewModel::effectiveTextFilter(const QString& key) const {
        const auto expression = textFilter(key);
        if (key.trimmed() != executorColumnKey() || quickSector_.trimmed().isEmpty()) {
            return expression;
        }
        return QString::fromStdString(filterpanel::executorFilterWithQuickSector(
            expression.toStdString(), quickSector_.toStdString()));
    }

    bool AdvancedTextFilterViewModel::publishExpression(const QString& key,
                                                        const QString& expression,
                                                        const bool inferOperatorMode) {
        if (!state_.setTextFilter(key, expression)) {
            return false;
        }
        columns_.setExpression(
            {.key = key, .expression = expression, .inferOperatorMode = inferOperatorMode});
        publishChangedFor(key);
        return true;
    }

    bool AdvancedTextFilterViewModel::publishExpressionAndApply(const QString& key,
                                                                const QString& expression,
                                                                const bool inferOperatorMode) {
        if (!publishExpression(key, expression, inferOperatorMode)) {
            return false;
        }
        emit expressionApplied(key, expression);
        emit applyRequested();
        return true;
    }

    void AdvancedTextFilterViewModel::publishChanged() {
        rebuildCardStates();
        ++version_;
        emit changed();
    }

    void AdvancedTextFilterViewModel::publishChangedFor(const QString& key) {
        updateCardState(key);
        ++version_;
        emit changed();
    }

    void AdvancedTextFilterViewModel::rebuildCardStates() {
        QVariantList states;
        QHash<QString, int> indexes;
        states.reserve(rows_.size());
        for (const auto& row : rows_) {
            auto state = createCardState(row.toMap());
            const auto key = state.value("key").toString();
            indexes.insert(key, static_cast<int>(states.size()));
            states.push_back(state);
        }
        cardStates_ = std::move(states);
        cardStateIndex_ = std::move(indexes);
    }

    void AdvancedTextFilterViewModel::updateCardState(const QString& key) {
        const auto index = cardStateIndex_.constFind(key);
        if (index == cardStateIndex_.constEnd() || index.value() < 0 ||
            index.value() >= cardStates_.size()) {
            rebuildCardStates();
            return;
        }
        const auto state = createCardState(cardStates_.at(index.value()).toMap());
        cardStates_[index.value()] = state;
    }

    QVariantMap AdvancedTextFilterViewModel::createCardState(const QVariantMap& baseState) const {
        auto state = baseState;
        const auto key = state.value("key").toString();
        const auto mode = operatorModeFor(key);
        const auto modeIndex = operatorModeIndex_.constFind(mode);
        const int operatorIndex =
            modeIndex == operatorModeIndex_.constEnd() ? -1 : modeIndex.value();
        QString label;
        if (operatorIndex >= 0) {
            label = operatorModes_.at(operatorIndex).toMap().value("label").toString();
        } else if (mode == "mixed") {
            label = tr("Misto");
        }
        state.insert("textFilter", effectiveTextFilter(key));
        state.insert("operatorIndex", operatorIndex);
        state.insert("operatorLabel", label);
        return state;
    }

} // namespace ssa::presentation
