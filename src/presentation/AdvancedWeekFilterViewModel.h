#pragma once

#include "presentation/FilterPanelAdvancedState.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace ssa::presentation {

    class AdvancedWeekFilterViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QStringList weekColumnKeys READ weekColumnKeys CONSTANT)
        Q_PROPERTY(QString weekColumnKey READ weekColumnKey WRITE setWeekColumnKey NOTIFY changed)
        Q_PROPERTY(QString yearFilter READ yearFilter WRITE setYearFilter NOTIFY changed)
        Q_PROPERTY(QString weekFilter READ weekFilter WRITE setWeekFilter NOTIFY changed)
        Q_PROPERTY(
            QString issueYearFilter READ issueYearFilter WRITE setIssueYearFilter NOTIFY changed)
        Q_PROPERTY(QString executionYearFilter READ executionYearFilter WRITE setExecutionYearFilter
                       NOTIFY changed)
        Q_PROPERTY(QString issueWeekStartFilter READ issueWeekStartFilter WRITE
                       setIssueWeekStartFilter NOTIFY changed)
        Q_PROPERTY(QString issueWeekEndFilter READ issueWeekEndFilter WRITE setIssueWeekEndFilter
                       NOTIFY changed)
        Q_PROPERTY(QString executionWeekStartFilter READ executionWeekStartFilter WRITE
                       setExecutionWeekStartFilter NOTIFY changed)
        Q_PROPERTY(QString executionWeekEndFilter READ executionWeekEndFilter WRITE
                       setExecutionWeekEndFilter NOTIFY changed)

      public:
        AdvancedWeekFilterViewModel(filterpanel::FilterPanelAdvancedState& state,
                                    QStringList weekColumnKeys, QObject* parent = nullptr);

        [[nodiscard]] QStringList weekColumnKeys() const;
        [[nodiscard]] QString weekColumnKey() const;
        void setWeekColumnKey(const QString& value);
        [[nodiscard]] QString yearFilter() const;
        void setYearFilter(const QString& value);
        [[nodiscard]] QString weekFilter() const;
        void setWeekFilter(const QString& value);
        [[nodiscard]] QString issueYearFilter() const;
        void setIssueYearFilter(const QString& value);
        [[nodiscard]] QString executionYearFilter() const;
        void setExecutionYearFilter(const QString& value);
        [[nodiscard]] QString issueWeekStartFilter() const;
        void setIssueWeekStartFilter(const QString& value);
        [[nodiscard]] QString issueWeekEndFilter() const;
        void setIssueWeekEndFilter(const QString& value);
        [[nodiscard]] QString executionWeekStartFilter() const;
        void setExecutionWeekStartFilter(const QString& value);
        [[nodiscard]] QString executionWeekEndFilter() const;
        void setExecutionWeekEndFilter(const QString& value);
        Q_INVOKABLE [[nodiscard]] bool isYearValid(const QString& value) const;
        Q_INVOKABLE [[nodiscard]] bool isWeekValid(const QString& value) const;
        Q_INVOKABLE [[nodiscard]] bool isYearWeekValid(const QString& value) const;
        void refreshFromState();

      signals:
        void changed();

      private:
        filterpanel::FilterPanelAdvancedState& state_;
        QStringList weekColumnKeys_;
    };

} // namespace ssa::presentation
