#pragma once

#include "ports/IExternalCommandPort.h"

#include <QObject>
#include <QString>

#include <memory>

namespace ssa::presentation {

    class CommandViewModel final : public QObject {
        Q_OBJECT

      public:
        explicit CommandViewModel(std::shared_ptr<ports::IExternalCommandPort> port,
                                  QObject* parent = nullptr);

      public slots:
        void openSamHome();
        void openSsa(const QString& numeroSsa);

      private:
        std::shared_ptr<ports::IExternalCommandPort> port_;
    };

} // namespace ssa::presentation
