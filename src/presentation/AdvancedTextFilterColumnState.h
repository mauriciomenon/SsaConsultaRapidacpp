#pragma once

#include "query/TextFilterToken.h"

#include <QString>

#include <string>

namespace ssa::presentation {

    struct AdvancedTextFilterColumnState {
        query::TextFilterTokenSet tokens;
        QString operatorMode{
            QString::fromStdString(query::textFilterUiModeName(query::TextFilterUiMode::Equals))};
        QString snapshot;
        std::string rawSnapshot;
    };

} // namespace ssa::presentation
