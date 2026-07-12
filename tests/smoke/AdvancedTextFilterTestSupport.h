#pragma once

#include "presentation/AdvancedTextFilterViewModel.h"

#include <QByteArray>
#include <QStringList>
#include <QVariantMap>

namespace ssa::tests {

    [[nodiscard]] inline QStringList
    advancedTextFilterKeys(const presentation::AdvancedTextFilterViewModel& model) {
        const auto roles = model.roleNames();
        const int keyRole = roles.key(QByteArrayLiteral("key"), -1);
        QStringList keys;
        keys.reserve(model.rowCount());
        for (int row = 0; row < model.rowCount(); ++row) {
            keys.push_back(model.data(model.index(row, 0), keyRole).toString());
        }
        return keys;
    }

    [[nodiscard]] inline QVariantMap
    advancedTextFilterCardState(const presentation::AdvancedTextFilterViewModel& model,
                                const QString& key) {
        const auto roles = model.roleNames();
        const int keyRole = roles.key(QByteArrayLiteral("key"), -1);
        for (int row = 0; row < model.rowCount(); ++row) {
            const auto index = model.index(row, 0);
            if (model.data(index, keyRole).toString() != key) {
                continue;
            }
            QVariantMap state;
            for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
                state.insert(QString::fromUtf8(role.value()), model.data(index, role.key()));
            }
            return state;
        }
        return {};
    }

} // namespace ssa::tests
