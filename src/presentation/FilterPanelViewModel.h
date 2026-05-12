#pragma once

#include "domain/SsaTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <string>

namespace ssa::presentation {

    class FilterPanelViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString quickSector READ quickSector WRITE setQuickSector NOTIFY changed)
        Q_PROPERTY(
            bool excludeScaSesSte READ excludeScaSesSte WRITE setExcludeScaSesSte NOTIFY changed)
        Q_PROPERTY(QStringList filterColumnKeys READ filterColumnKeys CONSTANT)
        Q_PROPERTY(QString columnKey READ columnKey WRITE setColumnKey NOTIFY changed)
        Q_PROPERTY(QString columnValue READ columnValue WRITE setColumnValue NOTIFY changed)
        Q_PROPERTY(QStringList activeFilters READ activeFilters NOTIFY changed)
        Q_PROPERTY(QString activeFilterSummary READ activeFilterSummary NOTIFY changed)

      public:
        explicit FilterPanelViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString quickSector() const;
        void setQuickSector(const QString& value);
        [[nodiscard]] bool excludeScaSesSte() const;
        void setExcludeScaSesSte(bool value);
        [[nodiscard]] QStringList filterColumnKeys() const;
        [[nodiscard]] QString columnKey() const;
        void setColumnKey(const QString& value);
        [[nodiscard]] QString columnValue() const;
        void setColumnValue(const QString& value);
        [[nodiscard]] QStringList activeFilters() const;
        [[nodiscard]] QString activeFilterSummary() const;
        [[nodiscard]] std::map<std::string, std::string> columnFilters() const;
        void setColumnFilters(std::map<std::string, std::string> filters);

      signals:
        void changed();
        void applyRequested();

      public slots:
        void addColumnFilter();
        void resetFilters();

      private:
        void rebuildActiveFilters();
        void markActiveFiltersDirty();
        void ensureActiveFiltersUpToDate() const;

        QString quickSector_;
        bool excludeScaSesSte_{domain::kDefaultExcludeScaSesSte};
        QStringList filterColumnKeys_;
        QString columnKey_;
        QString columnValue_;
        std::map<std::string, std::string> columnFilters_;
        mutable QStringList activeFilters_;
        mutable QString activeFilterSummary_;
        mutable bool activeFiltersStale_{true};
    };

} // namespace ssa::presentation
