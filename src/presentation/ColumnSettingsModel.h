#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QAbstractListModel>
#include <QString>

#include <map>
#include <string>
#include <string_view>
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
            VisibilityChangeEnabledRole,
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
        Q_INVOKABLE void setFilterText(const QString& filterText);

        void applyPreferences(const std::vector<std::string>& visibleColumns,
                              const std::map<std::string, int>& columnWidths);
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        [[nodiscard]] std::vector<std::string> visibleKeys() const;
        [[nodiscard]] std::map<std::string, int> columnWidths() const;

      signals:
        void changed();

      private:
        struct ColumnItem {
            std::string key;
            std::string label;
            std::string keyLower;
            std::string labelLower;
            bool defaultVisible{false};
            bool visible{false};
            int defaultWidth{132};
            int width{132};
        };

        [[nodiscard]] int visibleCount() const;
        [[nodiscard]] std::vector<ColumnItem>::iterator findColumn(std::string_view key);
        [[nodiscard]] std::vector<ColumnItem>::const_iterator
        findColumn(std::string_view key) const;
        [[nodiscard]] bool setColumnVisibleBySourceRow(std::size_t row, bool visible);
        [[nodiscard]] bool matchesFilter(const ColumnItem& column) const;
        [[nodiscard]] int sourceRowFromModelRow(int modelRow) const;
        [[nodiscard]] int modelRowFromSourceRow(std::size_t sourceRow) const;
        void rebuildFilteredRows();
        void emitRowChanged(int row);

        std::vector<ColumnItem> columns_;
        std::vector<int> filteredRows_;
        std::vector<int> sourceToModelRow_;
        int visibleCount_{0};
        std::string filterTextLower_{};
    };

} // namespace ssa::presentation
