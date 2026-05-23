#pragma once

#include "presentation/AdvancedTextFilterColumnStore.h"
#include "presentation/FilterPanelAdvancedState.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace ssa::presentation {

    class AdvancedTextFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList rows READ rows CONSTANT)
        Q_PROPERTY(QVariantList cardStates READ cardStates NOTIFY changed)
        Q_PROPERTY(QVariantList operatorModes READ operatorModes CONSTANT)
        Q_PROPERTY(int version READ version NOTIFY changed)

      public:
        explicit AdvancedTextFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                             QObject* parent = nullptr);

        [[nodiscard]] const QVariantList& rows() const;
        [[nodiscard]] const QVariantList& cardStates() const;
        [[nodiscard]] const QVariantList& operatorModes() const;
        [[nodiscard]] int version() const;

        Q_INVOKABLE [[nodiscard]] QString operatorModeFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QString operatorLabelFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] int operatorIndexFor(const QString& key) const;
        Q_INVOKABLE void setOperatorMode(const QString& key, const QString& operatorMode);
        Q_INVOKABLE [[nodiscard]] QString textFilter(const QString& key) const;
        Q_INVOKABLE void setTextFilter(const QString& key, const QString& value);
        Q_INVOKABLE bool updateFilterWithSelectedValue(const QString& key, const QString& value);
        Q_INVOKABLE void replaceWithOperatorValueList(const QString& key, const QStringList& values,
                                                      const QString& operatorMode);
        void refreshFromState();

      signals:
        void changed();

      private:
        bool publishExpression(const QString& key, const QString& expression,
                               bool inferOperatorMode = false);
        void publishChanged();
        void publishChangedFor(const QString& key);
        void rebuildCardStates();
        void updateCardState(const QString& key);
        QVariantMap createCardState(const QVariantMap& baseState) const;

        filterpanel::FilterPanelAdvancedState& state_;
        QVariantList rows_;
        QVariantList cardStates_;
        QHash<QString, int> cardStateIndex_;
        QVariantList operatorModes_;
        QHash<QString, int> operatorModeIndex_;
        AdvancedTextFilterColumnStore columns_;
        int version_{0};
    };

} // namespace ssa::presentation
