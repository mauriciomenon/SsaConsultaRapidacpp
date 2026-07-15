#include "presentation/AdvancedTextFilterColumnStore.h"

#include <map>
#include <string>

namespace ssa::presentation {

    QString AdvancedTextFilterColumnStore::operatorModeFor(const QString& key) const {
        const auto columnEntry = columns_.find(key);
        return columnEntry == columns_.end() ? QString{"equals"} : columnEntry->second.operatorMode;
    }

    bool AdvancedTextFilterColumnStore::setOperatorMode(const OperatorModeUpdate& update) {
        const auto normalizedMode = QString::fromStdString(domain::textFilterUiModeName(
            domain::textFilterUiModeFromName(update.operatorMode.toStdString())));
        auto [columnEntry, inserted] = columns_.try_emplace(update.key);
        if (inserted && !update.currentExpression.trimmed().isEmpty()) {
            columnEntry->second.tokens =
                domain::parseTextFilterTokens(update.currentExpression.toStdString());
            columnEntry->second.snapshot = update.currentExpression.trimmed();
        }
        auto& column = columnEntry->second;
        if (column.operatorMode == normalizedMode) {
            return false;
        }
        column.operatorMode = normalizedMode;
        return true;
    }

    void AdvancedTextFilterColumnStore::setExpression(const ExpressionUpdate& update) {
        auto columnEntry = columns_.try_emplace(update.key).first;
        auto& column = columnEntry->second;
        const auto rawExpression = update.expression.toStdString();
        if (column.rawSnapshot != rawExpression) {
            column.tokens = domain::parseTextFilterTokens(rawExpression);
            column.rawSnapshot = rawExpression;
        }
        if (update.inferOperatorMode && !column.tokens.ordered.empty()) {
            column.operatorMode =
                QString::fromStdString(domain::textFilterUiModeNameForTokens(column.tokens));
        }
        column.snapshot = update.expression.trimmed();
    }

    std::optional<QString>
    AdvancedTextFilterColumnStore::expressionWithAddedValue(const AddValueRequest& request) {
        auto tokens = domain::parseTextFilterTokens(request.currentExpression.toStdString());
        auto filterOperator =
            domain::textFilterOperatorFromMode(request.operatorMode.toStdString());
        if (!filterOperator.has_value() ||
            !domain::addTextFilterValue(tokens, request.value.toStdString(), *filterOperator)) {
            return std::nullopt;
        }
        return QString::fromStdString(domain::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingWithOperator(
        const OperatorValueListRequest& request) {
        const auto filterOperator =
            domain::textFilterOperatorFromMode(request.operatorMode.toStdString());
        if (!filterOperator.has_value()) {
            return {};
        }
        domain::TextFilterTokenSet tokens;
        for (const auto& value : request.values) {
            domain::addTextFilterValue(tokens, value.toStdString(), *filterOperator);
        }
        return QString::fromStdString(domain::joinTextFilterTokens(tokens));
    }

    QString AdvancedTextFilterColumnStore::expressionReplacingWithOperatorLists(
        const OperatorValueListsRequest& request) {
        domain::TextFilterTokenSet tokens;
        for (const auto& value : request.includeValues) {
            domain::addTextFilterValue(tokens, value.toStdString(),
                                       domain::TextFilterOperator::Equals);
        }
        for (const auto& value : request.excludeValues) {
            domain::addTextFilterValue(tokens, value.toStdString(),
                                       domain::TextFilterOperator::Different);
        }
        return QString::fromStdString(domain::joinTextFilterTokens(tokens));
    }

    bool
    AdvancedTextFilterColumnStore::refreshFrom(const std::map<std::string, std::string>& filters) {
        return synchronizer_.refresh(columns_, filters);
    }

} // namespace ssa::presentation
