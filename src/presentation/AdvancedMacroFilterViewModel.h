#pragma once

#include "presentation/FilterPanelAdvancedState.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class AdvancedMacroFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList options READ options CONSTANT)
        Q_PROPERTY(QString selectedMacro READ selectedMacro WRITE setSelectedMacro NOTIFY changed)
        Q_PROPERTY(QString reportTitle READ reportTitle NOTIFY changed)
        Q_PROPERTY(QString reportText READ reportText NOTIFY changed)
        Q_PROPERTY(QVariantList reportRows READ reportRows NOTIFY changed)

      public:
        AdvancedMacroFilterViewModel(filterpanel::FilterPanelAdvancedState& advancedState,
                                     const filterpanel::FilterPanelState& filterState,
                                     std::shared_ptr<query::SsaQueryService> queryService,
                                     QObject* parent = nullptr);

        [[nodiscard]] const QVariantList& options() const;
        [[nodiscard]] QString selectedMacro() const;
        void setSelectedMacro(const QString& value);
        [[nodiscard]] QString reportTitle() const;
        [[nodiscard]] QString reportText() const;
        [[nodiscard]] const QVariantList& reportRows() const;
        void refreshFromState();

      signals:
        void changed();

      private:
        void applyBaixarPreset();
        void buildExecutadasReport(const QString& value);
        void clearReport();

        filterpanel::FilterPanelAdvancedState& advancedState_;
        const filterpanel::FilterPanelState& filterState_;
        std::shared_ptr<query::SsaQueryService> queryService_;
        QVariantList options_;
        QString selectedMacro_;
        QString reportTitle_;
        QString reportText_;
        QVariantList reportRows_;
    };

} // namespace ssa::presentation
