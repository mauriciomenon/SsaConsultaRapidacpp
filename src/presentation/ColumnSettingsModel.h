#pragma once

#include <QAbstractListModel>
#include <QString>

#include <map>
#include <string>
#include <vector>

namespace ssa::presentation {

    class ColumnSettingsModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(int minColumnWidth READ minColumnWidth CONSTANT)
        Q_PROPERTY(int maxColumnWidth READ maxColumnWidth CONSTANT)

      public:
        enum Role {
            KeyRole = Qt::UserRole + 1,
            LabelRole,
            VisibleRole,
            WidthRole,
            ToggleEnabledRole,
        };

        explicit ColumnSettingsModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] int minColumnWidth() const;
        [[nodiscard]] int maxColumnWidth() const;

        Q_INVOKABLE bool setColumnVisible(int row, bool visible);
        Q_INVOKABLE bool setColumnVisibleByKey(const QString& columnKey, bool visible);
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
        int visibleCount_{0};
    };

} // namespace ssa::presentation
