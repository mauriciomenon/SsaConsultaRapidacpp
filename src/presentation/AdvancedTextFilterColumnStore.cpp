#include "presentation/AdvancedTextFilterColumnStore.h"

namespace ssa::presentation {

    QString AdvancedTextFilterColumnStore::operatorModeFor(const QString& key) const {
        const auto it = columns_.find(key);
        return it == columns_.end() ? QString{"equals"} : it->second.operatorMode;
    }

    bool AdvancedTextFilterColumnStore::setOperatorMode(const QString& key,
                                                        const QString& operatorMode,
                                                        const QString& currentExpression) {
        const auto normalizedMode = QString::fromStdString(query::textFilterUiModeName(
            query::textFilterUiModeFromName(operatorMode.toStdString())));
        auto [it, inserted] = columns_.try_emplace(key);
        if (inserted && !currentExpression.trimmed().isEmpty()) {
            it->second.tokens = query::parseTextFilterTokens(currentExpression.toStdString());
            it->second.snapshot = currentExpression.trimmed();
        }
        auto& column = it->second;
        if (column.operatorMode == normalizedMode) {
            return false;
        }
        column.operatorMode = normalizedMode;
        return true;
    }

    query::TextFilterTokenSet
    AdvancedTextFilterColumnStore::tokensFor(const QString& key, const QString& expression) const {
        const auto it = columns_.find(key);
        if (it != columns_.end()) {
            return it->second.tokens;
        }
        return query::parseTextFilterTokens(expression.toStdString());
    }

    void AdvancedTextFilterColumnStore::setExpression(const QString& key, const QString& expression,
                                                      const bool inferOperatorMode) {
        auto it = columns_.try_emplace(key).first;
        auto& column = it->second;
        const auto rawExpression = expression.toStdString();
        if (column.rawSnapshot != rawExpression) {
            column.tokens = query::parseTextFilterTokens(rawExpression);
            column.rawSnapshot = rawExpression;
        }
        if (inferOperatorMode && !column.tokens.ordered.empty()) {
            column.operatorMode =
                QString::fromStdString(query::textFilterUiModeNameForTokens(column.tokens));
        }
        column.snapshot = expression.trimmed();
    }

    std::optional<QString> AdvancedTextFilterColumnStore::expressionWithAddedValue(
        const QString& key, const QString& currentExpression, const QString& value,
        const QString& operatorMode) const {
        auto tokens = tokensFor(key, currentExpression);
        auto op = query::textFilterOperatorFromMode(operatorMode.toStdString());
        if (!op.has_value() || !query::addTextFilterValue(tokens, value.toStdString(), *op)) {
            return std::nullopt;
        }
        return QString::fromStdString(query::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingCurrentExpressionWithOperator(
        const QString& currentExpression, const QString& operatorMode) {
        const auto op = query::textFilterOperatorFromMode(operatorMode.toStdString());
        if (!op.has_value()) {
            return currentExpression;
        }
        const auto tokens = query::parseTextFilterTokens(currentExpression.toStdString());
        query::TextFilterTokenSet replaced;
        for (const auto& token : tokens.ordered) {
            query::addTextFilterValue(replaced, token.value, *op);
        }
        return QString::fromStdString(query::joinTextFilterTokens(replaced));
    }

    QString
    AdvancedTextFilterColumnStore::expressionReplacingWithOperator(const QStringList& values,
                                                                   const QString& operatorMode) {
        const auto op = query::textFilterOperatorFromMode(operatorMode.toStdString());
        if (!op.has_value()) {
            return {};
        }
        query::TextFilterTokenSet tokens;
        for (const auto& value : values) {
            query::addTextFilterValue(tokens, value.toStdString(), *op);
        }
        return QString::fromStdString(query::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingWithOperatorLists(
        const QStringList& includeValues, const QStringList& excludeValues) {
        query::TextFilterTokenSet tokens;
        for (const auto& value : includeValues) {
            query::addTextFilterValue(tokens, value.toStdString(),
                                      query::TextFilterOperator::Equals);
        }
        for (const auto& value : excludeValues) {
            query::addTextFilterValue(tokens, value.toStdString(),
                                      query::TextFilterOperator::Different);
        }
        return QString::fromStdString(query::joinTextFilterTokens(tokens));
    }

    bool
    AdvancedTextFilterColumnStore::refreshFrom(const std::map<std::string, std::string>& filters) {
        return synchronizer_.refresh(columns_, filters);
    }

} // namespace ssa::presentation
