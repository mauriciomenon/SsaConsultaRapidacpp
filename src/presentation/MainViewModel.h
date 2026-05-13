#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"
#include "presentation/CommandViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/UiSettingsViewModel.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "query/SsaQueryService.h"

#include <QObject>

#include <map>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class MainViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(BrowseViewModel* browse READ browse CONSTANT)
        Q_PROPERTY(CommandViewModel* commands READ commands CONSTANT)
        Q_PROPERTY(ExportViewModel* exports READ exports CONSTANT)
        Q_PROPERTY(ColumnSettingsModel* columns READ columns CONSTANT)
        Q_PROPERTY(UiSettingsViewModel* ui READ ui CONSTANT)

      public:
        MainViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                      std::shared_ptr<ports::IExternalCommandPort> commandPort,
                      std::shared_ptr<ports::IUserPreferencesStore> preferencesStore = nullptr,
                      std::shared_ptr<application::SsaWorkflowService> workflowService = nullptr,
                      QObject* parent = nullptr);

        [[nodiscard]] BrowseViewModel* browse();
        [[nodiscard]] CommandViewModel* commands();
        [[nodiscard]] ExportViewModel* exports();
        [[nodiscard]] ColumnSettingsModel* columns();
        [[nodiscard]] UiSettingsViewModel* ui();

      signals:
        void pageChanged();
        void sortChanged();
        void preferencesChanged();

      public slots:
        void applyColumnSettings();
        void resetColumnSettings();
        void discardColumnSettings();
        void openSelectedSsa();
        void cancelCurrentRequest();

      private:
        [[nodiscard]] ports::UserPreferencesSnapshot buildPreferencesSnapshot() const;
        void applyPreferences(ports::UserPreferencesSnapshot snapshot);
        void scheduleSavePreferences();
        void savePreferences();

        BrowseViewModel browse_;
        CommandViewModel commands_;
        ExportViewModel exports_;
        ColumnSettingsModel columns_;
        UiSettingsViewModel ui_;
        UserPreferencesCoordinator preferences_;
    };

} // namespace ssa::presentation
