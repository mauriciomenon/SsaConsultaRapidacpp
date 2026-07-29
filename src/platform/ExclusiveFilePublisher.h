#pragma once

#include "ports/IDatabaseSwitchPorts.h"

namespace ssa::platform {

    [[nodiscard]] ports::ExclusiveFilePublishResult
    publishFileExclusively(const std::filesystem::path& source,
                           const std::filesystem::path& destination);

} // namespace ssa::platform
