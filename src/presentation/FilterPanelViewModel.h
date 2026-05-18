#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/ColumnFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelDistinctValuesController.h"
#include "presentation/FilterPanelSectorViewModel.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    struct ColumnValueOptionCacheEntry final {
        std::vector<std::string> source;
        QStringList options;
        QStringList previewSource;
    };

    class FilterPanelViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QStringList filterColumnKeys READ filterColumnKeys CONSTANT)
        Q_PROPERTY(QString columnKey READ columnKey WRITE setColumnKey NOTIFY changed)
        Q_PROPERTY(QString columnValue READ columnValue WRITE setColumnValue NOTIFY changed)
        Q_PROPERTY(QStringList weekColumnKeys READ weekColumnKeys CONSTANT)
        Q_PROPERTY(QObject* advanced READ advanced CONSTANT)
        Q_PROPERTY(QObject* columns READ columns CONSTANT)
        Q_PROPERTY(QObject* sector READ sector CONSTANT)
        Q_PROPERTY(int columnValueOptionsVersion READ columnValueOptionsVersion NOTIFY
                       columnValueOptionsChanged)
        Q_PROPERTY(QStringList activeFilters READ activeFilters NOTIFY changed)
        Q_PROPERTY(QString activeFilterSummary READ activeFilterSummary NOTIFY changed)

      public:
        explicit FilterPanelViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                                      QObject* parent = nullptr);

        [[nodiscard]] QString quickSector() const;
        void setQuickSector(const QString& value);
        [[nodiscard]] bool excludeScaSesSte() const;
        void setExcludeScaSesSte(bool value);
        [[nodiscard]] QStringList filterColumnKeys() const;
        [[nodiscard]] QString columnKey() const;
        void setColumnKey(const QString& value);
        [[nodiscard]] QString columnValue() const;
        void setColumnValue(const QString& value);
        [[nodiscard]] QStringList weekColumnKeys() const;
        [[nodiscard]] QObject* advanced() const;
        [[nodiscard]] QObject* columns();
        [[nodiscard]] QObject* sector();
        [[nodiscard]] int columnValueOptionsVersion() const;
        [[nodiscard]] QStringList quickSectorOptions() const;
        [[nodiscard]] QStringList quickSectorSelectorValues() const;
        [[nodiscard]] int quickSectorSelectorIndex() const;
        [[nodiscard]] QStringList activeFilters() const;
        [[nodiscard]] QString activeFilterSummary() const;
        [[nodiscard]] std::map<std::string, std::string> columnFilters() const;
        [[nodiscard]] domain::AdvancedFilterSpec advancedFilters() const;
        Q_INVOKABLE [[nodiscard]] bool hasFilterForColumn(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QStringList columnValueOptionsFor(const QString& key) const;
        Q_INVOKABLE [[nodiscard]] QStringList
        columnValuePreviewOptionsFor(const QString& key, int limit, bool expanded) const;
        Q_INVOKABLE [[nodiscard]] bool hasMoreColumnValueOptionsFor(const QString& key,
                                                                    int limit) const;
        Q_INVOKABLE [[nodiscard]] bool columnValueOptionsLoadingFor(const QString& key) const;
        Q_INVOKABLE void refreshColumnValueOptions();
        Q_INVOKABLE void refreshColumnValueOptionsFor(const QString& key);
        void setColumnFilters(std::map<std::string, std::string> filters);
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

      signals:
        void changed();
        void columnValueOptionsChanged();
        void applyRequested();

      public slots:
        void addColumnFilter();
        void resetFilters();

      private:
        void loadFilterCatalog();
        void configureDistinctValueRefresh();
        void refreshActiveFilters();
        void rebuildActiveFilters();
        void synchronizeFilterState(bool refreshSectorOptions);
        void setColumnValueOptions(const std::vector<std::string>& options, const QString& key);
        void clearColumnValueOptionsCache();
        void publishFilterStateChange(bool quickSectorChanged = false);
        void scheduleActiveFilterRefresh();
        void scheduleColumnValueRefresh();
        void refreshQuickSectorOptions();
        void markActiveFiltersDirty();

        filterpanel::FilterPanelState state_;
        QStringList filterColumnKeys_;
        QStringList weekColumnKeys_;
        ColumnFilterViewModel columns_;
        FilterPanelSectorViewModel sector_;
        FilterPanelAdvancedViewModel* advanced_{nullptr};
        QStringList activeFilters_;
        QString activeFilterSummary_;
        bool activeFiltersDirty_{true};
        std::map<QString, ColumnValueOptionCacheEntry> columnValueOptionsByKey_;
        QSet<QString> columnValueLoadingKeys_;
        int columnValueOptionsVersion_{0};
        FilterPanelDistinctValuesController distinctValues_;
        QTimer activeFilterRefreshTimer_;
    };

} // namespace ssa::presentation
