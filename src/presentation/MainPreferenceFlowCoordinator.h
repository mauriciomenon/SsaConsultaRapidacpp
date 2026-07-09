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
#include <QVariantList>

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
        Q_PROPERTY(QVariantList savedFilters READ savedFilters NOTIFY savedFiltersChanged)

      public:
        MainPreferenceFlowCoordinator(BrowseViewModel& browse, UiSettingsViewModel& ui,
                                      ColumnSettingsModel& columns,
                                      UserPreferencesCoordinator& preferences,
                                      std::shared_ptr<ports::IFilterPresetStore> presetStore,
                                      application::FilterPresetService& presetService,
                                      QObject* parent = nullptr);
        ~MainPreferenceFlowCoordinator() override;

        [[nodiscard]] ports::UserPreferencesSnapshot buildPreferencesSnapshot() const;
        [[nodiscard]] QVariantList savedFilters() const;
        void applyStoredPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void scheduleSavePreferences();
        void saveAppliedColumnPreferences(std::vector<std::string> visibleColumns,
                                          std::map<std::string, int> columnWidths);
        void saveNowOrSchedule();
        Q_INVOKABLE void savePreferences();
        Q_INVOKABLE QString suggestedFilterName() const;
        // True when the current filter state has at least one active filter
        // (search/column/advanced/exclusion). Used by QML to validate BEFORE
        // opening the save-filter dialog, mirroring the Python reference.
        Q_INVOKABLE bool hasActiveFilter() const;
        // Emits statusMessageRequested("Aplique algum filtro antes de salvar")
        // so QML can call it when hasActiveFilter() is false, without opening
        // the save dialog. Mirrors the Python QMessageBox.information flow.
        Q_INVOKABLE void notifyNoActiveFilter();
        Q_INVOKABLE void saveCurrentFilter(const QString& name);
        Q_INVOKABLE void applySavedFilter(const QString& name);
        Q_INVOKABLE void removeSavedFilter(const QString& name);
        Q_INVOKABLE void exportFilterPreset(const QUrl& outputUrl);
        Q_INVOKABLE void importFilterPreset(const QUrl& inputUrl);

      signals:
        void savedFiltersChanged();
        void statusMessageRequested(const QString& message);
        void statusErrorRequested(const QString& message);
        void statusErrorClearRequested();

      public slots:
        void requestSaveFromSignal();

      private:
        void finishExportFilterPreset();
        void finishImportFilterPreset();
        void waitForPresetTasks();
        void setSavedFilters(std::vector<ports::SavedFilterSnapshot> filters);

        BrowseViewModel& browse_;
        UiSettingsViewModel& ui_;
        ColumnSettingsModel& columns_;
        UserPreferencesCoordinator& preferences_;
        std::shared_ptr<ports::IFilterPresetStore> presetStore_;
        application::FilterPresetService& presetService_;
        std::vector<ports::SavedFilterSnapshot> savedFilters_;
        QFutureWatcher<QString> exportPresetWatcher_;
        QFutureWatcher<FilterPresetLoadResult> importPresetWatcher_;
    };

} // namespace ssa::presentation
