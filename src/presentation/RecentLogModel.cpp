#include "presentation/RecentLogModel.h"

#include <QDateTime>
#include <QMetaObject>
#include <QThread>

namespace ssa::presentation {

    RecentLogModel::RecentLogModel(QObject* parent) : QAbstractListModel(parent) {
        entries_.reserve(kMaximumEntries);
    }

    int RecentLogModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(entries_.size());
    }

    QVariant RecentLogModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }
        const auto& entry = entries_[static_cast<std::size_t>(index.row())];
        switch (role) {
        case TimestampRole:
            return entry.timestamp;
        case SeverityRole:
            return entry.severity;
        case SourceRole:
            return entry.source;
        case MessageRole:
            return entry.message;
        case DetailRole:
            return entry.detail;
        case FullTextRole:
            return fullText(entry);
        default:
            return {};
        }
    }

    QHash<int, QByteArray> RecentLogModel::roleNames() const {
        return {{TimestampRole, "timestamp"}, {SeverityRole, "severity"},
                {SourceRole, "source"},       {MessageRole, "message"},
                {DetailRole, "detail"},       {FullTextRole, "fullText"}};
    }

    void RecentLogModel::append(const QString& severity, const QString& source,
                                const QString& message, const QString& detail) {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(
                this,
                [this, severity, source, message, detail] {
                    append(severity, source, message, detail);
                },
                Qt::QueuedConnection);
            return;
        }
        if (message.isEmpty() && detail.isEmpty()) {
            return;
        }
        if (!entries_.empty()) {
            const auto& latest = entries_.front();
            if (latest.severity == severity && latest.source == source &&
                latest.message == message && latest.detail == detail) {
                return;
            }
        }

        beginInsertRows({}, 0, 0);
        entries_.insert(entries_.begin(), {QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                           severity, source, message, detail});
        endInsertRows();
        if (rowCount() > kMaximumEntries) {
            beginRemoveRows({}, kMaximumEntries, kMaximumEntries);
            entries_.pop_back();
            endRemoveRows();
        }
        emit countChanged();
    }

    QString RecentLogModel::entryText(const int row) const {
        if (row < 0 || row >= rowCount()) {
            return {};
        }
        return fullText(entries_[static_cast<std::size_t>(row)]);
    }

    QString RecentLogModel::allText() const {
        QString result;
        for (const auto& entry : entries_) {
            if (!result.isEmpty()) {
                result += QStringLiteral("\n\n");
            }
            result += fullText(entry);
        }
        return result;
    }

    QString RecentLogModel::fullText(const Entry& entry) {
        QString result = QStringLiteral("%1 [%2] %3\n%4")
                             .arg(entry.timestamp, entry.severity, entry.source, entry.message);
        if (!entry.detail.isEmpty()) {
            result += QStringLiteral("\n") + entry.detail;
        }
        return result;
    }

} // namespace ssa::presentation
