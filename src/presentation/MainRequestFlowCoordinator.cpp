#include "presentation/MainRequestFlowCoordinator.h"

#include "presentation/BrowseViewModel.h"

namespace ssa::presentation {

    MainRequestFlowCoordinator::MainRequestFlowCoordinator(BrowseViewModel& browse, QObject* parent)
        : QObject(parent), browse_(browse) {}

    void MainRequestFlowCoordinator::cancelCurrentRequest() {
        browse_.cancelCurrentRequest();
    }

} // namespace ssa::presentation
