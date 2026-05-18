#include "presentation/FilterPanelSectorViewModel.h"

#include <QSet>

namespace ssa::presentation {

    namespace {

        QStringList toStringList(const std::vector<std::string>& values) {
            QStringList list;
            list.reserve(static_cast<int>(values.size()));
            for (const auto& value : values) {
                list.append(QString::fromStdString(value));
            }
            return list;
        }

    } // namespace

    FilterPanelSectorViewModel::FilterPanelSectorViewModel(filterpanel::FilterPanelState& state,
                                                           QObject* parent)
        : QObject(parent), state_(state) {
        refreshSelector();
    }

    QString FilterPanelSectorViewModel::quickSector() const {
        return state_.quickSector();
    }

    void FilterPanelSectorViewModel::setQuickSector(const QString& value) {
        if (!state_.setQuickSector(value)) {
            return;
        }
        if (selectorValueSet_.contains(state_.quickSector())) {
            updateSelectorIndex();
        } else {
            appendTransientSectorValue(state_.quickSector());
        }
        emit stateChanged(true);
        emit changed();
    }

    bool FilterPanelSectorViewModel::excludeScaSesSte() const {
        return state_.excludeScaSesSte();
    }

    void FilterPanelSectorViewModel::setExcludeScaSesSte(const bool value) {
        if (!state_.setExcludeScaSesSte(value)) {
            return;
        }
        emit stateChanged(false);
        emit changed();
    }

    QStringList FilterPanelSectorViewModel::options() const {
        return options_;
    }

    QStringList FilterPanelSectorViewModel::selectorValues() const {
        return selectorValues_;
    }

    int FilterPanelSectorViewModel::selectorIndex() const {
        return selectorIndex_;
    }

    void FilterPanelSectorViewModel::setOptions(const std::vector<std::string>& values) {
        if (optionSource_ == values) {
            return;
        }
        optionSource_ = values;
        options_ = toStringList(values);
        rebuildSelectorValues();
    }

    void FilterPanelSectorViewModel::refreshSelector() {
        rebuildSelectorValues();
    }

    void FilterPanelSectorViewModel::rebuildSelectorValues() {
        QStringList values;
        QSet<QString> seen;
        QHash<QString, int> indexes;
        values.push_back("");
        seen.insert("");
        indexes.insert("", 0);

        const QString current = state_.quickSector();
        int selectedIndex = 0;
        bool selectedIndexFound = current.isEmpty();
        for (const auto& value : options_) {
            if (!seen.contains(value)) {
                values.push_back(value);
                seen.insert(value);
                indexes.insert(value, static_cast<int>(values.size() - 1));
            }
            if (value == current && !selectedIndexFound) {
                selectedIndex = indexes.value(value, 0);
                selectedIndexFound = true;
            }
        }

        if (!current.isEmpty() && !seen.contains(current)) {
            values.push_back(current);
            seen.insert(current);
            indexes.insert(current, static_cast<int>(values.size() - 1));
            selectedIndex = static_cast<int>(values.size() - 1);
        }

        if (selectorValues_ == values && selectorIndex_ == selectedIndex) {
            return;
        }
        const bool indexChanged = selectorIndex_ != selectedIndex;
        selectorValues_ = values;
        selectorValueSet_ = seen;
        selectorValueIndex_ = indexes;
        selectorIndex_ = selectedIndex;
        emit optionsChanged();
        if (indexChanged) {
            emit selectorIndexChanged();
        }
    }

    void FilterPanelSectorViewModel::updateSelectorIndex() {
        const int selectedIndex = selectorValueIndex_.value(state_.quickSector(), 0);
        if (selectorIndex_ == selectedIndex) {
            return;
        }
        selectorIndex_ = selectedIndex;
        emit selectorIndexChanged();
    }

    void FilterPanelSectorViewModel::appendTransientSectorValue(const QString& value) {
        selectorValues_.push_back(value);
        selectorValueSet_.insert(value);
        selectorIndex_ = static_cast<int>(selectorValues_.size() - 1);
        selectorValueIndex_.insert(value, selectorIndex_);
        emit optionsChanged();
        emit selectorIndexChanged();
    }

    void FilterPanelSectorViewModel::refreshFromState() {
        refreshSelector();
        emit changed();
    }

} // namespace ssa::presentation
