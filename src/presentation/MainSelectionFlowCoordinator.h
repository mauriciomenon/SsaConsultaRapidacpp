#pragma once

#include <QObject>

namespace ssa::presentation {

    class BrowseViewModel;
    class CommandViewModel;

    class MainSelectionFlowCoordinator final : public QObject {
        Q_OBJECT

      public:
        MainSelectionFlowCoordinator(BrowseViewModel& browse, CommandViewModel& commands,
                                     QObject* parent = nullptr);

      public slots:
        void openSelectedSsa();

      private:
        BrowseViewModel& browse_;
        CommandViewModel& commands_;
    };

} // namespace ssa::presentation
