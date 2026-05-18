#pragma once

#include <QObject>

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

      private:
        BrowseViewModel& browse_;
        ColumnSettingsModel& columns_;
        SaveTrigger savePreferences_;
    };

} // namespace ssa::presentation
