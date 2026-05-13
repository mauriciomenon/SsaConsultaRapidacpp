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

    void StatusViewModel::setQueryComplete(const qlonglong totalRows, const int pageNumber,
                                           const int pageCount) {
        if (totalRows == 0) {
            setMessage("0 registros");
            return;
        }
        setMessage(
            QString("%1 registros, pagina %2 de %3").arg(totalRows).arg(pageNumber).arg(pageCount));
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
