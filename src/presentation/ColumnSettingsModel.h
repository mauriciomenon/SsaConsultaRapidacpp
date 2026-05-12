#pragma once

#include <QAbstractListModel>
#include <QString>

#include <map>
#include <string>
#include <vector>

namespace ssa::presentation {

    class ColumnSettingsModel final : public QAbstractListModel {
        Q_OBJECT

      public:
        enum Role {
            KeyRole = Qt::UserRole + 1,
            LabelRole,
            VisibleRole,
            WidthRole,
        };

        explicit ColumnSettingsModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE void setColumnVisible(int row, bool visible);
        Q_INVOKABLE void setColumnWidth(const QString& columnKey, int width);
        Q_INVOKABLE void resetDefaults();
        Q_INVOKABLE void selectAll();

        void applyPreferences(const std::vector<std::string>& visibleColumns,
                              const std::map<std::string, int>& columnWidths);
        [[nodiscard]] std::vector<std::string> visibleKeys() const;
        [[nodiscard]] std::map<std::string, int> columnWidths() const;

      signals:
        void changed();

      private:
        struct ColumnItem {
            std::string key;
            std::string label;
            bool defaultVisible{false};
            bool visible{false};
            int defaultWidth{132};
            int width{132};
        };

        [[nodiscard]] int visibleCount() const;
        void emitRowChanged(int row);

        std::vector<ColumnItem> columns_;
    };

} // namespace ssa::presentation
