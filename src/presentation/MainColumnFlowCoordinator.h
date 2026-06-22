#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace ssa::presentation {

    class BrowseViewModel;
    class ColumnSettingsModel;

    class MainColumnFlowCoordinator final : public QObject {
        Q_OBJECT

      public:
        using SaveTrigger = std::function<void()>;

        MainColumnFlowCoordinator(BrowseViewModel& browse, ColumnSettingsModel& columns,
                                  SaveTrigger savePreferences, QObject* parent = nullptr);

      public slots:
        void applyColumnSettings();
        void resetColumnSettings();
        void discardColumnSettings();
        [[nodiscard]] bool setColumnWidthAndApply(const QString& columnKey, int width);
        [[nodiscard]] bool setColumnVisibleAndApply(const QString& columnKey, bool visible);
        [[nodiscard]] bool canHideColumn(const QString& columnKey) const;

      private:
        BrowseViewModel& browse_;
        ColumnSettingsModel& columns_;
        SaveTrigger savePreferences_;
    };

} // namespace ssa::presentation
