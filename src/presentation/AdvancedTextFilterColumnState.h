#pragma once

#include "domain/TextFilterToken.h"

#include <QString>

#include <string>

namespace ssa::presentation {

    struct AdvancedTextFilterColumnState {
        domain::TextFilterTokenSet tokens;
        QString operatorMode{
            QString::fromStdString(domain::textFilterUiModeName(domain::TextFilterUiMode::Equals))};
        QString snapshot;
        std::string rawSnapshot;
    };

} // namespace ssa::presentation
