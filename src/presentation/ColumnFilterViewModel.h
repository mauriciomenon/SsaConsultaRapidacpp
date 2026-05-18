#pragma once

#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace ssa::presentation {

    class ColumnFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
        Q_PROPERTY(int activeFilterCount READ activeFilterCount NOTIFY activeFilterCountChanged)

      public:
        explicit ColumnFilterViewModel(filterpanel::FilterPanelState& state,
                                       QObject* parent = nullptr);

        [[nodiscard]] QVariantList rows() const;
        [[nodiscard]] int activeFilterCount() const;
        Q_INVOKABLE bool applyFilterFor(const QString& key, const QString& value);
        Q_INVOKABLE bool clearFilterFor(const QString& key);
        void refreshFromState();

      signals:
        void rowsChanged();
        void activeFilterCountChanged();
        void stateChanged();
        void applyRequested();

      private:
        struct RowData {
            std::string keyStd;
            QString key;
            QString label;
            QString value;
        };

        [[nodiscard]] static QVariantMap rowToMap(const RowData& row);
        void loadRows();
        void refreshRowValues();

        filterpanel::FilterPanelState& state_;
        std::vector<RowData> rowData_;
        QVariantList rows_;
        int activeFilterCount_{0};
    };

} // namespace ssa::presentation
