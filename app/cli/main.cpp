#include "SsaCliApplicationRuntime.h"

#include <QCoreApplication>

#ifndef SSA_PROJECT_VERSION
#define SSA_PROJECT_VERSION "0.0.0"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ssa_consulta_rapida_cli");
    QCoreApplication::setApplicationVersion(SSA_PROJECT_VERSION);

    ssa::app::cli::SsaCliApplicationRuntime runtime;
    return runtime.run(QCoreApplication::arguments());
}
