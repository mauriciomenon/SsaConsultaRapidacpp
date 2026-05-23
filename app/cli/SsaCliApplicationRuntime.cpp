#include "SsaCliApplicationRuntime.h"

namespace ssa::app::cli {

    int SsaCliApplicationRuntime::run(const QStringList& arguments) {
        return serviceFactory_.createController().run(arguments);
    }

} // namespace ssa::app::cli
