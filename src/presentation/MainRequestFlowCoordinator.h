#pragma once

#include <QObject>

namespace ssa::presentation {

    class BrowseViewModel;

    class MainRequestFlowCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit MainRequestFlowCoordinator(BrowseViewModel& browse, QObject* parent = nullptr);

      public slots:
        void cancelCurrentRequest();

      private:
        BrowseViewModel& browse_;
    };

} // namespace ssa::presentation
