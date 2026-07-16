#pragma once

#include "domain/SsaTypes.h"
#include "ports/IExecutadasReportPort.h"
#include "ports/ISsaBrowsePort.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/ColumnFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelColumnValueOptions.h"
#include "presentation/FilterPanelDistinctValuesController.h"
#include "presentation/FilterPanelSectorViewModel.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ssa::presentation {

    class FilterPanelViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QStringList statusShortcutValues READ statusShortcutValues CONSTANT)
        Q_PROPERTY(QString excludedStatusCodesText READ excludedStatusCodesText CONSTANT)
        Q_PROPERTY(QString columnKey READ columnKey WRITE setColumnKey NOTIFY changed)
        Q_PROPERTY(QString columnValue READ columnValue WRITE setColumnValue NOTIFY changed)
        Q_PROPERTY(
            bool excludeScaSesSte READ excludeScaSesSte WRITE setExcludeScaSesSte NOTIFY changed)
        Q_PROPERTY(QStringList weekColumnKeys READ weekColumnKeys CONSTANT)
        Q_PROPERTY(QObject* advanced READ advanced CONSTANT)
        Q_PROPERTY(QObject* columns READ columns CONSTANT)
        Q_PROPERTY(QObject* sector READ sector CONSTANT)
        Q_PROPERTY(int focusColumnRequest READ focusColumnRequest NOTIFY focusColumnRequestChanged)
        Q_PROPERTY(QString activeFilterSummary READ activeFilterSummary NOTIFY changed)
        Q_PROPERTY(QVariantList activeFilterEntries READ activeFilterEntries NOTIFY changed)
        Q_PROPERTY(bool hasExclusionFilter READ hasExclusionFilter NOTIFY changed)

      public:
        explicit FilterPanelViewModel(
            std::shared_ptr<ports::ISsaBrowsePort> browsePort,
            std::shared_ptr<ports::IExecutadasReportPort> reportPort = nullptr,
            QObject* parent = nullptr);

        [[nodiscard]] QString quickSector() const;
        void setQuickSector(const QString& value);
        [[nodiscard]] bool excludeScaSesSte() const;
        void setExcludeScaSesSte(bool value);
        [[nodiscard]] QStringList statusShortcutValues() const;
        [[nodiscard]] QString excludedStatusCodesText() const;
        [[nodiscard]] QString columnKey() const;
        void setColumnKey(const QString& value);
        [[nodiscard]] QString columnValue() const;
        void setColumnValue(const QString& value);
        [[nodiscard]] QStringList weekColumnKeys() const;
        [[nodiscard]] QObject* advanced() const;
        [[nodiscard]] QObject* columns();
        [[nodiscard]] QObject* sector();
        [[nodiscard]] int focusColumnRequest() const;
        [[nodiscard]] QStringList quickSectorOptions() const;
        [[nodiscard]] QStringList quickSectorSelectorValues() const;
        [[nodiscard]] int quickSectorSelectorIndex() const;
        [[nodiscard]] QStringList activeFilters() const;
        [[nodiscard]] QString activeFilterSummary() const;
        [[nodiscard]] QVariantList activeFilterEntries() const;
        [[nodiscard]] bool hasExclusionFilter() const;
        [[nodiscard]] const std::map<std::string, std::string>& columnFilters() const;
        [[nodiscard]] domain::AdvancedFilterSpec advancedFilters() const;
        void requestColumnFocus(const QString& key);
        Q_INVOKABLE [[nodiscard]] bool hasFilterForColumn(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QStringList columnValueOptionsFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] int columnValueMaxLengthFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] bool columnValueOptionsLoadingFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QString columnValueOptionsErrorFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] bool statusShortcutSelected(const QString& code) const;
        // Returns the cycle state of a status shortcut value:
        // 0 = None (not filtered), 1 = Included (=CODE), 2 = Excluded (!CODE).
        // Dedicated contract so QML can render three visual states without
        // parsing activeFilterSummary.
        Q_INVOKABLE [[nodiscard]] int statusShortcutState(const QString& code) const;
        Q_INVOKABLE void toggleStatusShortcut(const QString& code);
        Q_INVOKABLE void clearStatusShortcuts();
        Q_INVOKABLE bool removeActiveFilter(const QVariantMap& entry);
        Q_INVOKABLE void refreshColumnValueOptionsFor(const QString& key);
        Q_INVOKABLE void retryQuickSectorOptions();
        void invalidateDataSourceOptions();
        void setColumnFilters(std::map<std::string, std::string> filters);
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;
        [[nodiscard]] bool backgroundWorkRunning() const;
        void cancelBackgroundWork();

      signals:
        void changed();
        void columnValueOptionsChangedFor(QString key);
        void columnValueOptionsReset();
        void focusColumnRequestChanged();
        void applyRequested();
        void backgroundActivityChanged();

      public slots:
        void addColumnFilter();
        void resetFilters();

      private:
        void loadFilterCatalog();
        void configureDistinctValueRefresh();
        void refreshActiveFilters();
        void rebuildActiveFilters();
        void synchronizeFilterState(bool refreshSectorOptions);
        void setColumnValueOptions(const std::vector<std::string>& options,
                                   std::size_t maxValueLength, const QString& key,
                                   std::uint64_t stateVersion);
        void publishFilterStateChange(bool quickSectorChanged = false);
        void invalidateColumnValueOptions();
        void scheduleActiveFilterRefresh();
        void refreshQuickSectorOptions();
        void markActiveFiltersDirty();
        void syncAdvancedQuickSector();
        bool normalizeAdvancedFilterOverlap();
        void handleAdvancedTextFilterApplied(const QString& key, const QString& expression);

        filterpanel::FilterPanelState state_;
        std::shared_ptr<ports::ISsaBrowsePort> browsePort_;
        QStringList weekColumnKeys_;
        ColumnFilterViewModel columns_;
        FilterPanelSectorViewModel sector_;
        FilterPanelAdvancedViewModel* advanced_{nullptr};
        QStringList activeFilters_;
        QString activeFilterSummary_;
        QVariantList activeFilterEntries_;
        bool activeFiltersDirty_{true};
        FilterPanelColumnValueOptions columnValueOptions_;
        FilterPanelDistinctValuesController distinctValues_;
        QTimer activeFilterRefreshTimer_;
        std::uint64_t filterStateVersion_{0};
        int focusColumnRequest_{0};
        bool stateReplacementInProgress_{false};
    };

} // namespace ssa::presentation
