#pragma once

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace ssa::presentation {

    class RecentLogModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

      public:
        enum Role {
            TimestampRole = Qt::UserRole + 1,
            SeverityRole,
            SourceRole,
            MessageRole,
            DetailRole,
            FullTextRole,
        };

        explicit RecentLogModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        Q_INVOKABLE void append(const QString& severity, const QString& source,
                                const QString& message, const QString& detail = {});
        Q_INVOKABLE [[nodiscard]] QString entryText(int row) const;
        Q_INVOKABLE [[nodiscard]] QString allText() const;

      signals:
        void countChanged();

      private:
        struct Entry {
            QString timestamp;
            QString severity;
            QString source;
            QString message;
            QString detail;
        };

        [[nodiscard]] static QString fullText(const Entry& entry);

        static constexpr int kMaximumEntries = 30;
        std::vector<Entry> entries_;
    };

} // namespace ssa::presentation
