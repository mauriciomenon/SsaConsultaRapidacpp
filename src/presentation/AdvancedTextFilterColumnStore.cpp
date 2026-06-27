#include "presentation/AdvancedTextFilterColumnStore.h"

namespace ssa::presentation {

    QString AdvancedTextFilterColumnStore::operatorModeFor(const QString& key) const {
        const auto it = columns_.find(key);
        return it == columns_.end() ? QString{"equals"} : it->second.operatorMode;
    }

    bool AdvancedTextFilterColumnStore::setOperatorMode(const OperatorModeUpdate& update) {
        const auto normalizedMode = QString::fromStdString(query::textFilterUiModeName(
            query::textFilterUiModeFromName(update.operatorMode.toStdString())));
        auto [it, inserted] = columns_.try_emplace(update.key);
        if (inserted && !update.currentExpression.trimmed().isEmpty()) {
            it->second.tokens =
                query::parseTextFilterTokens(update.currentExpression.toStdString());
            it->second.snapshot = update.currentExpression.trimmed();
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

    void AdvancedTextFilterColumnStore::setExpression(const ExpressionUpdate& update) {
        auto it = columns_.try_emplace(update.key).first;
        auto& column = it->second;
        const auto rawExpression = update.expression.toStdString();
        if (column.rawSnapshot != rawExpression) {
            column.tokens = query::parseTextFilterTokens(rawExpression);
            column.rawSnapshot = rawExpression;
        }
        if (update.inferOperatorMode && !column.tokens.ordered.empty()) {
            column.operatorMode =
                QString::fromStdString(query::textFilterUiModeNameForTokens(column.tokens));
        }
        column.snapshot = update.expression.trimmed();
    }

    std::optional<QString>
    AdvancedTextFilterColumnStore::expressionWithAddedValue(const AddValueRequest& request) const {
        auto tokens = tokensFor(request.key, request.currentExpression);
        auto op = query::textFilterOperatorFromMode(request.operatorMode.toStdString());
        if (!op.has_value() ||
            !query::addTextFilterValue(tokens, request.value.toStdString(), *op)) {
            return std::nullopt;
        }
        return QString::fromStdString(query::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingCurrentExpressionWithOperator(
        const OperatorExpressionRequest& request) {
        const auto op = query::textFilterOperatorFromMode(request.operatorMode.toStdString());
        if (!op.has_value()) {
            return request.currentExpression;
        }
        const auto tokens = query::parseTextFilterTokens(request.currentExpression.toStdString());
        query::TextFilterTokenSet replaced;
        for (const auto& token : tokens.ordered) {
            query::addTextFilterValue(replaced, token.value, *op);
        }
        return QString::fromStdString(query::joinTextFilterTokens(replaced));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingWithOperator(
        const OperatorValueListRequest& request) {
        const auto op = query::textFilterOperatorFromMode(request.operatorMode.toStdString());
        if (!op.has_value()) {
            return {};
        }
        query::TextFilterTokenSet tokens;
        for (const auto& value : request.values) {
            query::addTextFilterValue(tokens, value.toStdString(), *op);
        }
        return QString::fromStdString(query::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingWithOperatorLists(
        const OperatorValueListsRequest& request) {
        query::TextFilterTokenSet tokens;
        for (const auto& value : request.includeValues) {
            query::addTextFilterValue(tokens, value.toStdString(),
                                      query::TextFilterOperator::Equals);
        }
        for (const auto& value : request.excludeValues) {
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
