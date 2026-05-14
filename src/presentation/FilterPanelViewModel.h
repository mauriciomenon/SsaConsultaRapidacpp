#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class FilterPanelViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString quickSector READ quickSector WRITE setQuickSector NOTIFY changed)
        Q_PROPERTY(
            bool excludeScaSesSte READ excludeScaSesSte WRITE setExcludeScaSesSte NOTIFY changed)
        Q_PROPERTY(QStringList filterColumnKeys READ filterColumnKeys CONSTANT)
        Q_PROPERTY(QString columnKey READ columnKey WRITE setColumnKey NOTIFY changed)
        Q_PROPERTY(QString columnValue READ columnValue WRITE setColumnValue NOTIFY changed)
        Q_PROPERTY(QStringList weekColumnKeys READ weekColumnKeys CONSTANT)
        Q_PROPERTY(QString weekColumnKey READ weekColumnKey WRITE setWeekColumnKey NOTIFY changed)
        Q_PROPERTY(QString yearFilter READ yearFilter WRITE setYearFilter NOTIFY changed)
        Q_PROPERTY(QString weekFilter READ weekFilter WRITE setWeekFilter NOTIFY changed)
        Q_PROPERTY(
            QString derivationMode READ derivationMode WRITE setDerivationMode NOTIFY changed)
        Q_PROPERTY(QStringList derivationModeOptions READ derivationModeOptions CONSTANT)
        Q_PROPERTY(
            bool onlyReprogrammed READ onlyReprogrammed WRITE setOnlyReprogrammed NOTIFY changed)
        Q_PROPERTY(
            QStringList columnValueOptions READ columnValueOptions NOTIFY columnValueOptionsChanged)
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
        [[nodiscard]] QString weekColumnKey() const;
        void setWeekColumnKey(const QString& value);
        [[nodiscard]] QString yearFilter() const;
        void setYearFilter(const QString& value);
        [[nodiscard]] QString weekFilter() const;
        void setWeekFilter(const QString& value);
        [[nodiscard]] QString derivationMode() const;
        void setDerivationMode(const QString& value);
        [[nodiscard]] QStringList derivationModeOptions() const;
        [[nodiscard]] bool onlyReprogrammed() const;
        void setOnlyReprogrammed(bool value);
        [[nodiscard]] QStringList columnValueOptions() const;
        [[nodiscard]] QStringList activeFilters() const;
        [[nodiscard]] QString activeFilterSummary() const;
        [[nodiscard]] std::map<std::string, std::string> columnFilters() const;
        [[nodiscard]] domain::AdvancedFilterSpec advancedFilters() const;
        Q_INVOKABLE [[nodiscard]] bool hasFilterForColumn(const QString& key) const;
        Q_INVOKABLE void refreshColumnValueOptions();
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
        void rebuildActiveFilters();
        void refreshActiveFilters();
        void setColumnValueOptions(std::vector<std::string> options);
        void onColumnValueOptionsReady(const std::uint64_t requestToken);
        [[nodiscard]] std::optional<domain::DistinctValuesRequest>
        buildDistinctValuesRequest(const QString& columnKey) const;

        std::shared_ptr<query::SsaQueryService> queryService_;
        QString quickSector_;
        bool excludeScaSesSte_{domain::kDefaultExcludeScaSesSte};
        QStringList filterColumnKeys_;
        QStringList weekColumnKeys_;
        QString columnKey_;
        QString columnValue_;
        QString weekColumnKey_;
        QString yearFilter_;
        QString weekFilter_;
        QString derivationMode_{"all"};
        const QStringList derivationModeOptions_{"all", "root", "derived"};
        bool onlyReprogrammed_{false};
        std::map<std::string, std::string> columnFilters_;
        QStringList activeFilters_;
        QString activeFilterSummary_;
        QStringList columnValueOptions_;
        std::uint64_t columnValueRequestToken_{0};
        QFutureWatcher<std::vector<std::string>> columnValueOptionsWatcher_;
    };

} // namespace ssa::presentation
