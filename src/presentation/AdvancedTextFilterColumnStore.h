#pragma once

#include "presentation/AdvancedTextFilterColumnState.h"
#include "presentation/AdvancedTextFilterSnapshotSynchronizer.h"

#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssa::presentation {

    class AdvancedTextFilterColumnStore final {
      public:
        struct OperatorModeUpdate {
            QString key;
            QString operatorMode;
            QString currentExpression;
        };

        struct ExpressionUpdate {
            QString key;
            QString expression;
            bool inferOperatorMode{false};
        };

        struct AddValueRequest {
            QString key;
            QString currentExpression;
            QString value;
            QString operatorMode;
        };

        struct OperatorExpressionRequest {
            QString currentExpression;
            QString operatorMode;
        };

        struct OperatorValueListRequest {
            QStringList values;
            QString operatorMode;
        };

        struct OperatorValueListsRequest {
            QStringList includeValues;
            QStringList excludeValues;
        };

        [[nodiscard]] QString operatorModeFor(const QString& key) const;
        bool setOperatorMode(const OperatorModeUpdate& update);
        void setExpression(const ExpressionUpdate& update);
        [[nodiscard]] std::optional<QString>
        expressionWithAddedValue(const AddValueRequest& request) const;
        [[nodiscard]] static QString
        expressionReplacingCurrentExpressionWithOperator(const OperatorExpressionRequest& request);
        [[nodiscard]] static QString
        expressionReplacingWithOperator(const OperatorValueListRequest& request);
        [[nodiscard]] static QString
        expressionReplacingWithOperatorLists(const OperatorValueListsRequest& request);
        bool refreshFrom(const std::map<std::string, std::string>& filters);

      private:
        std::map<QString, AdvancedTextFilterColumnState> columns_;
        AdvancedTextFilterSnapshotSynchronizer synchronizer_;
    };

} // namespace ssa::presentation
