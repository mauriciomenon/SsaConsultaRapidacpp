#pragma once

#include "platform/AppPaths.h"
#include "platform/StartupOptions.h"
#include "presentation/MainViewModel.h"

#include <memory>

namespace ssa::app::desktop {

    class DesktopMainViewModelFactory final {
      public:
        [[nodiscard]] static std::unique_ptr<ssa::presentation::MainViewModel>
        create(const ssa::platform::StartupOptions& options, const ssa::platform::AppPaths& paths);
    };

} // namespace ssa::app::desktop
