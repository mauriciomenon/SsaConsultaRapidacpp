#include "presentation/DerivadasGraphModel.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace ssa::presentation {

    namespace {

        // Mirrors gui/ssa/details_dialog_constants.py
        constexpr qreal kNodeWidth = 118;
        constexpr qreal kNodeHeight = 52;
        constexpr qreal kXGap = 170;
        constexpr qreal kYGap = kNodeHeight + 9;
        constexpr qreal kMargin = 8;

        QString mermaidEscaped(QString value) {
            value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
            value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
            value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
            return value;
        }

        QString mermaidNodeLabel(const QString& ssa, const QString& status) {
            if (status.isEmpty()) {
                return mermaidEscaped(ssa);
            }
            return mermaidEscaped(ssa + QStringLiteral("\n") + status);
        }

    } // namespace

    DerivadasGraphModel::DerivadasGraphModel(QObject* parent) : QAbstractListModel(parent) {}

    void DerivadasGraphModel::buildFromRelations(const QString& target,
                                                 const QVariantList& relations) {
        beginResetModel();
        nodes_.clear();
        edges_.clear();
        target_ = target.trimmed();

        if (target_.isEmpty()) {
            graphWidth_ = 0;
            graphHeight_ = 0;
            endResetModel();
            emit graphChanged();
            return;
        }

        // Build a directed graph: Current is the target; DerivedFrom points to
        // an ancestor (ancestor -> target); Related are dashed lateral edges.
        // Node order: ancestors first (depth 0..n-1), then target, then children.
        std::unordered_map<std::string, int> seenIndex;
        std::unordered_map<std::string, QString> statusBySsa;
        std::vector<QString> orderedSsa;
        std::vector<int> orderedDepth;

        const auto appendNode = [&](const QString& ssa, int depth) {
            if (ssa.isEmpty()) {
                return;
            }
            const auto key = ssa.toStdString();
            if (seenIndex.find(key) != seenIndex.end()) {
                return;
            }
            seenIndex.emplace(key, static_cast<int>(orderedSsa.size()));
            orderedSsa.push_back(ssa);
            orderedDepth.push_back(depth);
        };

        for (const auto& entry : relations) {
            const auto map = entry.toMap();
            const auto ssa = map.value(QStringLiteral("ssa")).toString();
            const auto status = map.value(QStringLiteral("status")).toString();
            if (!ssa.isEmpty() && !status.isEmpty()) {
                statusBySsa.insert_or_assign(ssa.toStdString(), status);
            }
        }

        // First pass: collect ancestors (DerivedFrom) then target, then children
        // (related nodes). Depth is heuristic from the relation kind since the
        // local record only exposes direct relations. Dedupe relation values so
        // a repeated SSA does not produce duplicate nodes or edges.
        std::vector<QString> ancestors;
        std::unordered_set<std::string> seenAncestors;
        for (const auto& entry : relations) {
            const auto map = entry.toMap();
            const auto kind = map.value(QStringLiteral("kind")).toString();
            const auto role = map.value(QStringLiteral("role")).toString();
            const auto ssa = map.value(QStringLiteral("ssa")).toString();
            const bool isParent = role == QStringLiteral("parent") ||
                                  (role.isEmpty() && (kind == QStringLiteral("Origem") ||
                                                      kind == QStringLiteral("Derivada de")));
            if (ssa.isEmpty() || !isParent) {
                continue;
            }
            if (seenAncestors.insert(ssa.toStdString()).second) {
                ancestors.push_back(ssa);
            }
        }

        int targetDepth = static_cast<int>(ancestors.size());
        for (int index = 0; index < static_cast<int>(ancestors.size()); ++index) {
            appendNode(ancestors[index], index);
        }
        appendNode(target_, targetDepth);

        std::vector<std::pair<QString, bool>> children;
        std::unordered_set<std::string> seenChildren;
        for (const auto& entry : relations) {
            const auto map = entry.toMap();
            const auto kind = map.value(QStringLiteral("kind")).toString();
            const auto role = map.value(QStringLiteral("role")).toString();
            const auto ssa = map.value(QStringLiteral("ssa")).toString();
            const bool isCurrent = role == QStringLiteral("current") ||
                                   (role.isEmpty() && kind == QStringLiteral("Atual"));
            const bool isParent = role == QStringLiteral("parent") ||
                                  (role.isEmpty() && (kind == QStringLiteral("Origem") ||
                                                      kind == QStringLiteral("Derivada de")));
            if (ssa.isEmpty() || isCurrent || isParent) {
                continue;
            }
            if (seenChildren.insert(ssa.toStdString()).second) {
                const bool isRelated = role == QStringLiteral("related") ||
                                       (role.isEmpty() && kind == QStringLiteral("Relacionada"));
                children.emplace_back(ssa, isRelated);
                appendNode(ssa, targetDepth + 1);
            }
        }

        // Layout: x = margin + depth * xGap; y = margin + index * yGap.
        for (std::size_t index = 0; index < orderedSsa.size(); ++index) {
            GraphNode node;
            node.ssa = orderedSsa[index];
            if (const auto statusIt = statusBySsa.find(node.ssa.toStdString());
                statusIt != statusBySsa.end()) {
                node.status = statusIt->second;
            }
            node.isTarget = node.ssa == target_;
            const qreal x = kMargin + orderedDepth[index] * kXGap;
            const qreal y = kMargin + static_cast<qreal>(index) * kYGap;
            node.position = QPointF(x, y);
            nodes_.push_back(std::move(node));
        }

        // Edges: each ancestor -> target; target -> each child.
        for (const auto& ancestor : ancestors) {
            GraphEdge edge;
            edge.from = ancestor;
            edge.to = target_;
            edges_.push_back(std::move(edge));
        }
        for (const auto& [child, dashed] : children) {
            GraphEdge edge;
            edge.from = target_;
            edge.to = child;
            edge.dashed = dashed;
            edges_.push_back(std::move(edge));
        }

        // Compute bounding box.
        qreal maxX = 0;
        qreal maxY = 0;
        for (const auto& node : nodes_) {
            maxX = std::max(maxX, node.position.x() + kNodeWidth);
            maxY = std::max(maxY, node.position.y() + kNodeHeight);
        }
        graphWidth_ = maxX + kMargin;
        graphHeight_ = maxY + kMargin;

        endResetModel();
        emit graphChanged();
    }

    int DerivadasGraphModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(nodes_.size());
    }

    QVariant DerivadasGraphModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(nodes_.size())) {
            return {};
        }
        const auto& node = nodes_[index.row()];
        switch (role) {
        case SsaRole:
            return node.ssa;
        case IsTargetRole:
            return node.isTarget;
        case PositionRole:
            return QPointF(node.position.x() + kNodeWidth / 2.0,
                           node.position.y() + kNodeHeight / 2.0);
        case LabelRole:
            return node.ssa;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> DerivadasGraphModel::roleNames() const {
        return {
            {SsaRole, "ssa"},
            {IsTargetRole, "isTarget"},
            {PositionRole, "nodeCenter"},
            {LabelRole, "label"},
        };
    }

    QString DerivadasGraphModel::target() const {
        return target_;
    }

    QString DerivadasGraphModel::summary() const {
        return QStringLiteral("Nos: %1 | Relacoes: %2")
            .arg(static_cast<int>(nodes_.size()))
            .arg(static_cast<int>(edges_.size()));
    }

    QString DerivadasGraphModel::mermaid() const {
        if (nodes_.empty()) {
            return QStringLiteral("flowchart LR\n");
        }
        QString result = QStringLiteral("flowchart LR\n");
        std::unordered_map<std::string, QString> idBySsa;
        idBySsa.reserve(nodes_.size());
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            const auto id = QStringLiteral("N%1").arg(static_cast<int>(index));
            idBySsa.emplace(nodes_[index].ssa.toStdString(), id);
            result += QStringLiteral("  %1[\"%2\"]\n")
                          .arg(id, mermaidNodeLabel(nodes_[index].ssa, nodes_[index].status));
        }
        for (const auto& edge : edges_) {
            const auto fromIt = idBySsa.find(edge.from.toStdString());
            const auto toIt = idBySsa.find(edge.to.toStdString());
            if (fromIt == idBySsa.end() || toIt == idBySsa.end()) {
                continue;
            }
            result += edge.dashed
                          ? QStringLiteral("  %1 -.-> %2\n").arg(fromIt->second, toIt->second)
                          : QStringLiteral("  %1 --> %2\n").arg(fromIt->second, toIt->second);
        }
        result +=
            QStringLiteral("  classDef target fill:#ffbf00,stroke:#4a3a00,stroke-width:2px\n");
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].isTarget) {
                result += QStringLiteral("  class N%1 target\n").arg(static_cast<int>(index));
            }
        }
        return result;
    }

    qreal DerivadasGraphModel::graphWidth() const {
        return graphWidth_;
    }

    qreal DerivadasGraphModel::graphHeight() const {
        return graphHeight_;
    }

    QVariantList DerivadasGraphModel::edges() const {
        QVariantList result;
        result.reserve(static_cast<qsizetype>(edges_.size()));
        std::unordered_map<std::string, QPointF> centerBySsa;
        for (const auto& node : nodes_) {
            centerBySsa.emplace(node.ssa.toStdString(),
                                QPointF(node.position.x() + kNodeWidth / 2.0,
                                        node.position.y() + kNodeHeight / 2.0));
        }
        for (const auto& edge : edges_) {
            const auto fromIt = centerBySsa.find(edge.from.toStdString());
            const auto toIt = centerBySsa.find(edge.to.toStdString());
            if (fromIt == centerBySsa.end() || toIt == centerBySsa.end()) {
                continue;
            }
            QVariantMap entry;
            entry.insert(QStringLiteral("fromX"), fromIt->second.x());
            entry.insert(QStringLiteral("fromY"), fromIt->second.y());
            entry.insert(QStringLiteral("toX"), toIt->second.x());
            entry.insert(QStringLiteral("toY"), toIt->second.y());
            entry.insert(QStringLiteral("dashed"), edge.dashed);
            result.push_back(entry);
        }
        return result;
    }

    QPointF DerivadasGraphModel::nodeCenter(const int index) const {
        if (index < 0 || index >= static_cast<int>(nodes_.size())) {
            return {};
        }
        const auto& node = nodes_[index];
        return {node.position.x() + kNodeWidth / 2.0, node.position.y() + kNodeHeight / 2.0};
    }

    QString DerivadasGraphModel::nodeSsa(const int index) const {
        if (index < 0 || index >= static_cast<int>(nodes_.size())) {
            return {};
        }
        return nodes_[index].ssa;
    }

    QString DerivadasGraphModel::nodeStatus(const int index) const {
        if (index < 0 || index >= static_cast<int>(nodes_.size())) {
            return {};
        }
        return nodes_[index].status;
    }

    bool DerivadasGraphModel::nodeIsTarget(const int index) const {
        if (index < 0 || index >= static_cast<int>(nodes_.size())) {
            return false;
        }
        return nodes_[index].isTarget;
    }

} // namespace ssa::presentation
