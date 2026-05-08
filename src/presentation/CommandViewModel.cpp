#include "presentation/CommandViewModel.h"

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    CommandViewModel::CommandViewModel(std::shared_ptr<ports::IExternalCommandPort> port,
                                       QObject* parent)
        : QObject(parent), port_(std::move(port)) {
        if (!port_) {
            throw std::invalid_argument("external command port is required");
        }
    }

    void CommandViewModel::openSamHome() {
        port_->openSamHome();
    }

    void CommandViewModel::openSsa(const QString& numeroSsa) {
        port_->openSsa(numeroSsa.toStdString());
    }

} // namespace ssa::presentation
