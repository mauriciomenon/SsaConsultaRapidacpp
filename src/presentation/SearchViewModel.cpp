#include "presentation/SearchViewModel.h"

namespace ssa::presentation {

    SearchViewModel::SearchViewModel(QObject* parent) : QObject(parent) {}

    QString SearchViewModel::text() const {
        return text_;
    }

    void SearchViewModel::setText(const QString& value) {
        if (text_ == value) {
            return;
        }
        text_ = value;
        emit textChanged();
    }

    void SearchViewModel::apply() {
        emit applyRequested();
    }

    void SearchViewModel::clear() {
        setText({});
        emit textClearRequested();
    }

} // namespace ssa::presentation
