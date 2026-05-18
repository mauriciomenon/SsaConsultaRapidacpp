#pragma once

#include "presentation/AdvancedTextFilterColumnState.h"

#include <QString>

#include <map>
#include <set>
#include <string>

namespace ssa::presentation {

    class AdvancedTextFilterSnapshotSynchronizer final {
      public:
        bool refresh(std::map<QString, AdvancedTextFilterColumnState>& columns,
                     const std::map<std::string, std::string>& filters) const;

      private:
        struct RefreshResult {
            std::set<QString> activeKeys;
            bool changed{false};
        };

        [[nodiscard]] RefreshResult
        refreshActiveColumns(std::map<QString, AdvancedTextFilterColumnState>& columns,
                             const std::map<std::string, std::string>& filters) const;
        [[nodiscard]] bool refreshColumn(std::map<QString, AdvancedTextFilterColumnState>& columns,
                                         const QString& columnKey, const std::string& value) const;
        [[nodiscard]] bool
        removeStaleColumns(std::map<QString, AdvancedTextFilterColumnState>& columns,
                           const std::set<QString>& activeKeys) const;
        [[nodiscard]] static QString utf8String(const std::string& value);
    };

} // namespace ssa::presentation
