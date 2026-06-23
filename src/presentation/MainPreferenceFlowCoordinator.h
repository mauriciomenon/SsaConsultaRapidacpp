#pragma once

#include "application/FilterPresetService.h"
#include "ports/IFilterPresetStore.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"
#include "presentation/UiSettingsViewModel.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ssa::presentation {

    class UserPreferencesCoordinator;

    struct FilterPresetLoadResult {
        ports::FilterPresetSnapshot snapshot;
        QString error;
    };

    class MainPreferenceFlowCoordinator final : public QObject {
        Q_OBJECT

      public:
        MainPreferenceFlowCoordinator(BrowseViewModel& browse, UiSettingsViewModel& ui,
                                      ColumnSettingsModel& columns,
                                      UserPreferencesCoordinator& preferences,
                                      std::shared_ptr<ports::IFilterPresetStore> presetStore,
                                      application::FilterPresetService& presetService,
                                      QObject* parent = nullptr);
        ~MainPreferenceFlowCoordinator() override;

        [[nodiscard]] ports::UserPreferencesSnapshot buildPreferencesSnapshot() const;
        void applyStoredPreferences(ports::UserPreferencesSnapshot snapshot);
        void scheduleSavePreferences();
        void saveAppliedColumnPreferences(std::vector<std::string> visibleColumns,
                                          std::map<std::string, int> columnWidths);
        void saveNowOrSchedule();
        Q_INVOKABLE void savePreferences();
        Q_INVOKABLE void exportFilterPreset(const QUrl& outputUrl);
        Q_INVOKABLE void importFilterPreset(const QUrl& inputUrl);

      signals:
        void statusMessageRequested(const QString& message);
        void statusErrorRequested(const QString& message);
        void statusErrorClearRequested();

      public slots:
        void requestSaveFromSignal();

      private:
        void finishExportFilterPreset();
        void finishImportFilterPreset();
        void waitForPresetTasks();

        BrowseViewModel& browse_;
        UiSettingsViewModel& ui_;
        ColumnSettingsModel& columns_;
        UserPreferencesCoordinator& preferences_;
        std::shared_ptr<ports::IFilterPresetStore> presetStore_;
        application::FilterPresetService& presetService_;
        QFutureWatcher<QString> exportPresetWatcher_;
        QFutureWatcher<FilterPresetLoadResult> importPresetWatcher_;
    };

} // namespace ssa::presentation
