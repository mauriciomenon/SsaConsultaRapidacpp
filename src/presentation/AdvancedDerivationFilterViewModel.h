#pragma once

#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace ssa::presentation {

    class AdvancedDerivationFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString reprogrammingEqualsFilter READ reprogrammingEqualsFilter WRITE
                       setReprogrammingEqualsFilter NOTIFY changed)
        Q_PROPERTY(
            QString derivationMode READ derivationMode WRITE setDerivationMode NOTIFY changed)
        Q_PROPERTY(QStringList derivationModeOptions READ derivationModeOptions CONSTANT)
        Q_PROPERTY(
            bool onlyReprogrammed READ onlyReprogrammed WRITE setOnlyReprogrammed NOTIFY changed)

      public:
        explicit AdvancedDerivationFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                                   QObject* parent = nullptr);

        [[nodiscard]] QString reprogrammingEqualsFilter() const;
        void setReprogrammingEqualsFilter(const QString& value);
        [[nodiscard]] QString derivationMode() const;
        void setDerivationMode(const QString& value);
        [[nodiscard]] QStringList derivationModeOptions() const;
        [[nodiscard]] bool onlyReprogrammed() const;
        void setOnlyReprogrammed(bool value);

      signals:
        void changed();

      private:
        filterpanel::FilterPanelAdvancedState& state_;
        const QStringList derivationModeOptions_{"all", "root", "derived"};
    };

} // namespace ssa::presentation
