#pragma once

#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace ssa::presentation {

    class AdvancedDerivationFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString reprogrammingMode READ reprogrammingMode WRITE setReprogrammingMode
                       NOTIFY changed)
        Q_PROPERTY(QStringList reprogrammingModeOptions READ reprogrammingModeOptions CONSTANT)
        Q_PROPERTY(QStringList reprogrammingValues READ reprogrammingValues WRITE
                       setReprogrammingValues NOTIFY changed)
        Q_PROPERTY(
            QString derivationMode READ derivationMode WRITE setDerivationMode NOTIFY changed)
        Q_PROPERTY(QStringList derivationModeOptions READ derivationModeOptions CONSTANT)
        Q_PROPERTY(
            bool onlyReprogrammed READ onlyReprogrammed WRITE setOnlyReprogrammed NOTIFY changed)

      public:
        explicit AdvancedDerivationFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                                   QObject* parent = nullptr);

        [[nodiscard]] QString reprogrammingMode() const;
        void setReprogrammingMode(const QString& value);
        [[nodiscard]] QStringList reprogrammingModeOptions() const;
        [[nodiscard]] QStringList reprogrammingValues() const;
        void setReprogrammingValues(const QStringList& values);
        [[nodiscard]] QString derivationMode() const;
        void setDerivationMode(const QString& value);
        [[nodiscard]] QStringList derivationModeOptions() const;
        [[nodiscard]] bool onlyReprogrammed() const;
        void setOnlyReprogrammed(bool value);
        void refreshFromState();

      signals:
        void changed();

      private:
        filterpanel::FilterPanelAdvancedState& state_;
        const QStringList reprogrammingModeOptions_{"eq", "lte", "gte"};
        const QStringList derivationModeOptions_{"all", "root", "derived"};
    };

} // namespace ssa::presentation
