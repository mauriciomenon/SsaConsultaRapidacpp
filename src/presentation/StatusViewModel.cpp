#include "presentation/StatusViewModel.h"

namespace ssa::presentation {

    StatusViewModel::StatusViewModel(QObject* parent) : QObject(parent) {}

    QString StatusViewModel::message() const {
        return message_;
    }

    QString StatusViewModel::error() const {
        return error_;
    }

    bool StatusViewModel::loading() const {
        return loading_;
    }

    void StatusViewModel::setMessage(const QString& value) {
        message_ = value;
        emit changed();
    }

    void StatusViewModel::setQueryComplete(const qlonglong totalRows, const qlonglong totalRowsAll,
                                           const int pageNumber, const int pageCount) {
        (void)totalRows;
        (void)totalRowsAll;
        if (totalRows == 0) {
            setMessage(QStringLiteral("Nenhum resultado"));
            return;
        }
        if (pageCount > 1) {
            setMessage(QStringLiteral("Pagina %1 de %2").arg(pageNumber).arg(pageCount));
            return;
        }
        setMessage(QStringLiteral("Consulta concluida"));
    }

    void StatusViewModel::setError(const QString& value) {
        error_ = value;
        emit changed();
    }

    void StatusViewModel::setLoading(const bool value) {
        loading_ = value;
        emit changed();
    }

} // namespace ssa::presentation
