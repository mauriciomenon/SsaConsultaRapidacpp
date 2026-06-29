#include "presentation/MainSelectionFlowCoordinator.h"

#include "presentation/BrowseViewModel.h"
#include "presentation/CommandViewModel.h"

namespace ssa::presentation {

    MainSelectionFlowCoordinator::MainSelectionFlowCoordinator(BrowseViewModel& browse,
                                                               CommandViewModel& commands,
                                                               QObject* parent)
        : QObject(parent), browse_(browse), commands_(commands) {}

    void MainSelectionFlowCoordinator::openSelectedSsa() {
        const auto selected = browse_.details()->selectedSsa();
        if (!selected.isEmpty()) {
            commands_.openSsa(selected);
        }
    }

    void MainSelectionFlowCoordinator::openSsa(const QString& numeroSsa) {
        const auto trimmed = numeroSsa.trimmed();
        if (!trimmed.isEmpty()) {
            commands_.openSsa(trimmed);
        }
    }

} // namespace ssa::presentation
