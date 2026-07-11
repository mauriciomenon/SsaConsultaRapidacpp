#pragma once

#include "presentation/AdvancedTextFilterColumnStore.h"
#include "presentation/FilterPanelAdvancedState.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace ssa::presentation {

    class AdvancedTextFilterViewModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(QVariantList operatorModes READ operatorModes CONSTANT)

      public:
        explicit AdvancedTextFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                             QObject* parent = nullptr);

        [[nodiscard]] const QVariantList& rows() const;
        [[nodiscard]] const QVariantList& cardStates() const;
        [[nodiscard]] const QVariantList& operatorModes() const;
        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE [[nodiscard]] QString operatorModeFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QString operatorLabelFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] int operatorIndexFor(const QString& key) const;
        Q_INVOKABLE void setOperatorMode(const QString& key, const QString& operatorMode);
        Q_INVOKABLE [[nodiscard]] QString textFilter(const QString& key) const;
        Q_INVOKABLE void setTextFilter(const QString& key, const QString& value);
        Q_INVOKABLE bool clearTextFilterAndApply(const QString& key);
        Q_INVOKABLE bool updateFilterWithSelectedValue(const QString& key, const QString& value);
        Q_INVOKABLE void replaceWithOperatorValueList(const QString& key, const QStringList& values,
                                                      const QString& operatorMode);
        Q_INVOKABLE void replaceWithOperatorValueLists(const QString& key,
                                                       const QStringList& includeValues,
                                                       const QStringList& excludeValues);
        void setQuickSector(QString value);
        void refreshFromState();

      signals:
        void changed();
        void applyRequested();
        void expressionApplied(QString key, QString expression);

      private:
        enum Role {
            KeyRole = Qt::UserRole + 1,
            LabelRole,
            LabelShortRole,
            TextFilterRole,
            OperatorIndexRole,
            OperatorLabelRole,
        };

        [[nodiscard]] QString effectiveTextFilter(const QString& key) const;
        bool publishExpression(const QString& key, const QString& expression,
                               bool inferOperatorMode = false);
        bool publishExpressionAndApply(const QString& key, const QString& expression,
                                       bool inferOperatorMode = false);
        void publishChanged();
        void publishChangedFor(const QString& key);
        void rebuildCardStates();
        int updateCardState(const QString& key);
        QVariantMap createCardState(const QVariantMap& baseState) const;

        filterpanel::FilterPanelAdvancedState& state_;
        QVariantList rows_;
        QVariantList cardStates_;
        QHash<QString, int> cardStateIndex_;
        QVariantList operatorModes_;
        QHash<QString, int> operatorModeIndex_;
        AdvancedTextFilterColumnStore columns_;
        QString quickSector_;
        inline static const QList<int> kDynamicRoles{TextFilterRole, OperatorIndexRole,
                                                     OperatorLabelRole};
    };

} // namespace ssa::presentation
