#pragma once

#include "ports/IUserPreferencesStore.h"
#include "presentation/ColumnSettingsModel.h"
#include "presentation/CommandViewModel.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/SearchViewModel.h"
#include "presentation/SsaTableModel.h"
#include "presentation/StatusViewModel.h"
#include "query/SsaQueryService.h"

#include <QObject>

#include <map>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class MainViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(SearchViewModel* search READ search CONSTANT)
        Q_PROPERTY(FilterPanelViewModel* filters READ filters CONSTANT)
        Q_PROPERTY(DetailsViewModel* details READ details CONSTANT)
        Q_PROPERTY(StatusViewModel* status READ status CONSTANT)
        Q_PROPERTY(CommandViewModel* commands READ commands CONSTANT)
        Q_PROPERTY(ColumnSettingsModel* columns READ columns CONSTANT)
        Q_PROPERTY(SsaTableModel* tableModel READ tableModel CONSTANT)
        Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY preferencesChanged)
        Q_PROPERTY(int pageIndex READ pageIndex NOTIFY pageChanged)
        Q_PROPERTY(int pageCount READ pageCount NOTIFY pageChanged)
        Q_PROPERTY(int totalRows READ totalRows NOTIFY pageChanged)
        Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageChanged)
        Q_PROPERTY(QString sortColumnKey READ sortColumnKey NOTIFY sortChanged)
        Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)

      public:
        MainViewModel(std::shared_ptr<query::SsaQueryService> queryService,
                      std::shared_ptr<ports::IExternalCommandPort> commandPort,
                      std::shared_ptr<ports::IUserPreferencesStore> preferencesStore = nullptr,
                      QObject* parent = nullptr);

        [[nodiscard]] SearchViewModel* search();
        [[nodiscard]] FilterPanelViewModel* filters();
        [[nodiscard]] DetailsViewModel* details();
        [[nodiscard]] StatusViewModel* status();
        [[nodiscard]] CommandViewModel* commands();
        [[nodiscard]] ColumnSettingsModel* columns();
        [[nodiscard]] SsaTableModel* tableModel();
        [[nodiscard]] QString theme() const;
        void setTheme(const QString& value);
        [[nodiscard]] int pageIndex() const;
        [[nodiscard]] int pageCount() const;
        [[nodiscard]] int totalRows() const;
        [[nodiscard]] int pageSize() const;
        void setPageSize(int value);
        [[nodiscard]] QString sortColumnKey() const;
        [[nodiscard]] bool sortAscending() const;

      signals:
        void pageChanged();
        void sortChanged();
        void preferencesChanged();

      public slots:
        void load();
        void apply();
        void clearSearch();
        void nextPage();
        void previousPage();
        void selectRow(int row);
        void sortByColumn(int column);
        void applyColumnSettings();
        void resetColumnSettings();
        void discardColumnSettings();
        void openSelectedSsa();
        void cancelCurrentRequest();

      private:
        [[nodiscard]] domain::SsaPageRequest buildRequest() const;
        void runRequest(const domain::SsaPageRequest& request);
        void loadPreferences();
        void savePreferences();

        std::shared_ptr<query::SsaQueryService> queryService_;
        std::shared_ptr<ports::IUserPreferencesStore> preferencesStore_;
        std::vector<std::string> visibleColumns_;
        std::map<std::string, int> columnWidths_;
        QString theme_{"system"};
        SearchViewModel search_;
        FilterPanelViewModel filters_;
        DetailsViewModel details_;
        StatusViewModel status_;
        CommandViewModel commands_;
        ColumnSettingsModel columns_;
        SsaTableModel tableModel_;
        domain::SortSpec sort_;
        std::size_t pageIndex_{0};
        std::size_t pageSize_{100};
        std::size_t totalRows_{0};
        int requestGeneration_{0};
    };

} // namespace ssa::presentation
