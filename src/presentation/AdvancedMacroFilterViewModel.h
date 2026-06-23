#pragma once

#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace ssa::presentation {

    class AdvancedMacroFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList options READ options CONSTANT)
        Q_PROPERTY(QString selectedMacro READ selectedMacro WRITE setSelectedMacro NOTIFY changed)
        Q_PROPERTY(QString reportTitle READ reportTitle NOTIFY changed)
        Q_PROPERTY(QString reportText READ reportText NOTIFY changed)

      public:
        explicit AdvancedMacroFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                              QObject* parent = nullptr);

        [[nodiscard]] const QVariantList& options() const;
        [[nodiscard]] QString selectedMacro() const;
        void setSelectedMacro(const QString& value);
        [[nodiscard]] QString reportTitle() const;
        [[nodiscard]] QString reportText() const;
        void refreshFromState();

      signals:
        void changed();

      private:
        void applyBaixarPreset();
        void buildExecutadasReport(const QString& value);
        void clearReport();

        filterpanel::FilterPanelAdvancedState& state_;
        QVariantList options_;
        QString selectedMacro_;
        QString reportTitle_;
        QString reportText_;
    };

} // namespace ssa::presentation
