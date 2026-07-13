#pragma once

#include "ports/OperationError.h"

#include <QDebug>

#include <exception>

namespace ssa::presentation {

    inline void logAsyncOperationError(const char* context, const std::exception_ptr& error) {
        if (!error) {
            return;
        }
        try {
            std::rethrow_exception(error);
        } catch (const ports::OperationError& exception) {
            qWarning().noquote() << context << QString::fromStdString(exception.diagnostic());
        } catch (const std::exception& exception) {
            qWarning().noquote() << context << exception.what();
        } catch (...) {
            qWarning().noquote() << context << "unknown exception";
        }
    }

} // namespace ssa::presentation
