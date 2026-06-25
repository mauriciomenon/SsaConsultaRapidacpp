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
        if (totalRows == 0) {
            setMessage(QStringLiteral("0 / %1 SSAs").arg(totalRowsAll));
            return;
        }
        (void)pageNumber;
        (void)pageCount;
        setMessage(QStringLiteral("%1 / %2 SSAs").arg(totalRows).arg(totalRowsAll));
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
