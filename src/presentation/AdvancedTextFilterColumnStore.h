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
        [[nodiscard]] QString operatorModeFor(const QString& key) const;
        bool setOperatorMode(const QString& key, const QString& operatorMode,
                             const QString& currentExpression);
        void setExpression(const QString& key, const QString& expression,
                           bool inferOperatorMode = false);
        [[nodiscard]] std::optional<QString>
        expressionWithAddedValue(const QString& key, const QString& currentExpression,
                                 const QString& value, const QString& operatorMode);
        [[nodiscard]] QString
        expressionReplacingCurrentExpressionWithOperator(const QString& currentExpression,
                                                         const QString& operatorMode) const;
        [[nodiscard]] QString expressionReplacingWithOperator(const QStringList& values,
                                                              const QString& operatorMode) const;
        bool refreshFrom(const std::map<std::string, std::string>& filters);

      private:
        [[nodiscard]] query::TextFilterTokenSet tokensFor(const QString& key,
                                                          const QString& expression) const;

        std::map<QString, AdvancedTextFilterColumnState> columns_;
        AdvancedTextFilterSnapshotSynchronizer synchronizer_;
    };

} // namespace ssa::presentation
