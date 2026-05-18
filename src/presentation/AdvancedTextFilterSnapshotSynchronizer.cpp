#include "presentation/AdvancedTextFilterSnapshotSynchronizer.h"

#include <QByteArray>

#include <set>
#include <utility>
#include <vector>

namespace ssa::presentation {
    bool AdvancedTextFilterSnapshotSynchronizer::refresh(
        std::map<QString, AdvancedTextFilterColumnState>& columns,
        const std::map<std::string, std::string>& filters) const {
        const auto refreshed = refreshActiveColumns(columns, filters);
        return removeStaleColumns(columns, refreshed.activeKeys) || refreshed.changed;
    }

    AdvancedTextFilterSnapshotSynchronizer::RefreshResult
    AdvancedTextFilterSnapshotSynchronizer::refreshActiveColumns(
        std::map<QString, AdvancedTextFilterColumnState>& columns,
        const std::map<std::string, std::string>& filters) const {
        RefreshResult result;
        for (const auto& [key, value] : filters) {
            const auto columnKey = utf8String(key);
            result.activeKeys.insert(columnKey);
            result.changed = refreshColumn(columns, columnKey, value) || result.changed;
        }
        return result;
    }

    bool AdvancedTextFilterSnapshotSynchronizer::refreshColumn(
        std::map<QString, AdvancedTextFilterColumnState>& columns, const QString& columnKey,
        const std::string& value) const {
        const auto columnIt = columns.find(columnKey);
        if (columnIt != columns.end() && columnIt->second.rawSnapshot == value) {
            return false;
        }
        const auto columnValue = utf8String(value).trimmed();
        auto tokens = query::parseTextFilterTokens(value);
        const auto operatorMode =
            QString::fromStdString(query::textFilterUiModeNameForTokens(tokens));
        auto column = AdvancedTextFilterColumnState{
            .tokens = std::move(tokens),
            .operatorMode = operatorMode,
            .snapshot = columnValue,
            .rawSnapshot = value,
        };
        columns.insert_or_assign(columnKey, std::move(column));
        return true;
    }

    bool AdvancedTextFilterSnapshotSynchronizer::removeStaleColumns(
        std::map<QString, AdvancedTextFilterColumnState>& columns,
        const std::set<QString>& activeKeys) const {
        bool changed = false;
        std::vector<QString> removedKeys;
        for (const auto& entry : columns) {
            if (!entry.second.snapshot.isEmpty() && !activeKeys.contains(entry.first)) {
                removedKeys.push_back(entry.first);
            }
        }
        for (const auto& key : removedKeys) {
            columns.erase(key);
            changed = true;
        }
        return changed;
    }

    QString AdvancedTextFilterSnapshotSynchronizer::utf8String(const std::string& value) {
        return QString::fromUtf8(QByteArray::fromStdString(value));
    }

} // namespace ssa::presentation
