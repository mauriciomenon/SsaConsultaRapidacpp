#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <string>

namespace ssa::presentation {

    class FilterPanelViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString quickSector READ quickSector WRITE setQuickSector NOTIFY changed)
        Q_PROPERTY(bool excludeClosedStatuses READ excludeClosedStatuses WRITE
                       setExcludeClosedStatuses NOTIFY changed)
        Q_PROPERTY(QString columnKey READ columnKey WRITE setColumnKey NOTIFY changed)
        Q_PROPERTY(QString columnValue READ columnValue WRITE setColumnValue NOTIFY changed)
        Q_PROPERTY(QStringList activeFilters READ activeFilters NOTIFY changed)

      public:
        explicit FilterPanelViewModel(QObject* parent = nullptr);

        [[nodiscard]] QString quickSector() const;
        void setQuickSector(const QString& value);
        [[nodiscard]] bool excludeClosedStatuses() const;
        void setExcludeClosedStatuses(bool value);
        [[nodiscard]] QString columnKey() const;
        void setColumnKey(const QString& value);
        [[nodiscard]] QString columnValue() const;
        void setColumnValue(const QString& value);
        [[nodiscard]] QStringList activeFilters() const;
        [[nodiscard]] std::map<std::string, std::string> columnFilters() const;
        void setColumnFilters(std::map<std::string, std::string> filters);

      signals:
        void changed();
        void applyRequested();

      public slots:
        void addColumnFilter();
        void clearFilters();

      private:
        QString quickSector_;
        bool excludeClosedStatuses_{true};
        QString columnKey_{"situacao"};
        QString columnValue_;
        std::map<std::string, std::string> columnFilters_;
    };

} // namespace ssa::presentation
