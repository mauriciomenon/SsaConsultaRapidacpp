#pragma once

#include "presentation/FilterPanelState.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

namespace ssa::presentation {

    class FilterPanelSectorViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString quickSector READ quickSector WRITE setQuickSector NOTIFY changed)
        Q_PROPERTY(
            bool excludeScaSesSte READ excludeScaSesSte WRITE setExcludeScaSesSte NOTIFY changed)
        Q_PROPERTY(QStringList options READ options NOTIFY optionsChanged)
        Q_PROPERTY(QStringList selectorValues READ selectorValues NOTIFY optionsChanged)
        Q_PROPERTY(int selectorIndex READ selectorIndex NOTIFY selectorIndexChanged)
        Q_PROPERTY(QString optionsError READ optionsError NOTIFY optionsErrorChanged)

      public:
        explicit FilterPanelSectorViewModel(filterpanel::FilterPanelState& state,
                                            QObject* parent = nullptr);

        [[nodiscard]] QString quickSector() const;
        void setQuickSector(const QString& value);
        [[nodiscard]] bool excludeScaSesSte() const;
        void setExcludeScaSesSte(bool value);
        [[nodiscard]] QStringList options() const;
        [[nodiscard]] QStringList selectorValues() const;
        [[nodiscard]] int selectorIndex() const;
        [[nodiscard]] QString optionsError() const;

        void setOptions(const std::vector<std::string>& values);
        void setOptionsError(const QString& message);
        void refreshSelector();
        void refreshFromState();

      signals:
        void changed();
        void optionsChanged();
        void selectorIndexChanged();
        void optionsErrorChanged();
        void stateChanged(bool quickSectorChanged);

      private:
        void rebuildSelectorValues();
        void updateSelectorIndex();
        void appendTransientSectorValue(const QString& value);

        filterpanel::FilterPanelState& state_;
        std::vector<std::string> optionSource_;
        QStringList options_;
        QStringList selectorValues_;
        QSet<QString> selectorValueSet_;
        QHash<QString, int> selectorValueIndex_;
        int selectorIndex_{0};
        QString optionsError_;
    };

} // namespace ssa::presentation
