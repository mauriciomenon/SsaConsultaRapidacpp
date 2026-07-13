#pragma once

#include "application/FilterPresetService.h"
#include "ports/IFilterPresetStore.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"
#include "presentation/UiSettingsViewModel.h"
#include "presentation/WorkflowCommandViewModel.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::presentation {

    class UserPreferencesCoordinator;

    struct FilterPresetLoadResult {
        ports::FilterPresetSnapshot snapshot;
        std::string error;
    };

    class MainPreferenceFlowCoordinator final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantList savedFilters READ savedFilters NOTIFY savedFiltersChanged)

      public:
        MainPreferenceFlowCoordinator(BrowseViewModel& browse, UiSettingsViewModel& ui,
                                      ColumnSettingsModel& columns,
                                      WorkflowCommandViewModel& workflows,
                                      UserPreferencesCoordinator& preferences,
                                      std::shared_ptr<ports::IFilterPresetStore> presetStore,
                                      application::FilterPresetService& presetService,
                                      QObject* parent = nullptr);
        ~MainPreferenceFlowCoordinator() override;

        [[nodiscard]] ports::UserPreferencesSnapshot buildPreferencesSnapshot() const;
        [[nodiscard]] QVariantList savedFilters() const;
        [[nodiscard]] bool running() const;
        [[nodiscard]] bool canceling() const;
        [[nodiscard]] bool canCancel() const;
        void shutdown();
        void cancel();
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
        void shutdownStarted();
        void stateChanged();
        void savedFiltersChanged();
        void statusMessageRequested(const QString& message);
        void statusErrorRequested(const QString& message);
        void statusErrorClearRequested();

      public slots:
        void requestSaveFromSignal();

      private:
        struct ExportPresetTaskState final {
            std::mutex mutex;
            std::string error;
            bool canceled{false};
        };

        struct ImportPresetTaskState final {
            std::mutex mutex;
            std::optional<ports::FilterPresetSnapshot> snapshot;
            std::string error;
            bool canceled{false};
        };

        void finishExportFilterPreset();
        void finishImportFilterPreset();
        void setSavedFilters(std::vector<ports::SavedFilterSnapshot> filters);

        BrowseViewModel& browse_;
        UiSettingsViewModel& ui_;
        ColumnSettingsModel& columns_;
        WorkflowCommandViewModel& workflows_;
        UserPreferencesCoordinator& preferences_;
        std::shared_ptr<ports::IFilterPresetStore> presetStore_;
        application::FilterPresetService& presetService_;
        std::vector<ports::SavedFilterSnapshot> savedFilters_;
        QFutureWatcher<void> exportPresetWatcher_;
        QFutureWatcher<void> importPresetWatcher_;
        std::shared_ptr<ExportPresetTaskState> exportPresetTask_;
        std::shared_ptr<ImportPresetTaskState> importPresetTask_;
        std::stop_source exportPresetStopSource_;
        std::stop_source importPresetStopSource_;
        bool shuttingDown_{false};
    };

} // namespace ssa::presentation
