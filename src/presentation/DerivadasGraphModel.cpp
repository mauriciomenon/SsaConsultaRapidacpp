#include "presentation/DerivadasGraphModel.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace ssa::presentation {

    namespace {

        // Mirrors gui/ssa/details_dialog_constants.py
        constexpr qreal kNodeWidth = 118;
        constexpr qreal kNodeHeight = 52;
        constexpr qreal kXGap = 145;
        constexpr qreal kYGap = kNodeHeight + 52;
        constexpr int kMaxChildrenPerRow = 4;
        constexpr qreal kChildRowGap = kNodeHeight + 34;
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

        QString svgEscaped(QString value) {
            value.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
            value.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
            value.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
            value.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
            return value;
        }

        int childColumnCount(const std::size_t childCount) {
            if (childCount == 0) {
                return 0;
            }
            return std::min(kMaxChildrenPerRow, static_cast<int>(childCount));
        }

        qreal routeXBetween(const qreal fromX, const qreal toX) {
            const qreal distance = std::abs(toX - fromX);
            const qreal offset = std::min<qreal>(28.0, distance / 2.0);
            return toX + (fromX <= toX ? -offset : offset);
        }

        qreal sourceExitX(const qreal fromX, const qreal toX) {
            const qreal direction = fromX <= toX ? 1.0 : -1.0;
            return fromX + direction * 26.0;
        }

        qreal targetApproachX(const qreal fromX, const qreal toX) {
            const qreal direction = fromX <= toX ? 1.0 : -1.0;
            return toX - direction * 14.0;
        }

        std::optional<qreal> lowerLaneY(const qreal fromY, const qreal toY) {
            if (toY <= fromY + kNodeHeight / 2.0) {
                return std::nullopt;
            }
            return toY - kNodeHeight / 2.0 - 16.0;
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
        std::unordered_map<std::string, QString> roleBySsa;
        std::vector<QString> orderedSsa;
        std::vector<int> orderedDepth;

        const auto appendNode = [&](const QString& ssa, int depth, const QString& role) {
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
            statusBySsa.try_emplace(key, QString{});
            roleBySsa.insert_or_assign(key, role);
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
            appendNode(ancestors[index], index, QStringLiteral("parent"));
        }
        appendNode(target_, targetDepth, QStringLiteral("current"));

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
                appendNode(ssa, targetDepth + 1,
                           isRelated ? QStringLiteral("related") : QStringLiteral("child"));
            }
        }

        std::unordered_map<std::string, QPointF> positionBySsa;
        positionBySsa.reserve(orderedSsa.size());
        const qreal targetX = kMargin + static_cast<qreal>(targetDepth) * kXGap;
        const qreal targetY = kMargin;
        for (int index = 0; index < static_cast<int>(ancestors.size()); ++index) {
            positionBySsa.emplace(ancestors[index].toStdString(),
                                  QPointF(kMargin + static_cast<qreal>(index) * kXGap, targetY));
        }
        positionBySsa.emplace(target_.toStdString(), QPointF(targetX, targetY));
        const int childColumns = childColumnCount(children.size());
        for (int index = 0; index < static_cast<int>(children.size()); ++index) {
            const auto& child = children[static_cast<std::size_t>(index)].first;
            const int row = childColumns > 0 ? index / childColumns : 0;
            const int column = childColumns > 0 ? index % childColumns : 0;
            positionBySsa.emplace(
                child.toStdString(),
                QPointF(targetX + kXGap + static_cast<qreal>(column) * kXGap,
                        targetY + kYGap + static_cast<qreal>(row) * kChildRowGap));
        }

        // Layout: ancestors stay on the target row; children fan out one level below
        // the target so sibling derivadas are not read as a chain.
        for (std::size_t index = 0; index < orderedSsa.size(); ++index) {
            GraphNode node;
            node.ssa = orderedSsa[index];
            if (const auto statusIt = statusBySsa.find(node.ssa.toStdString());
                statusIt != statusBySsa.end()) {
                node.status = statusIt->second;
            }
            if (const auto roleIt = roleBySsa.find(node.ssa.toStdString());
                roleIt != roleBySsa.end()) {
                node.role = roleIt->second;
            }
            node.isTarget = node.ssa == target_;
            if (const auto positionIt = positionBySsa.find(node.ssa.toStdString());
                positionIt != positionBySsa.end()) {
                node.position = positionIt->second;
            } else {
                const qreal x = kMargin + orderedDepth[index] * kXGap;
                const qreal y = kMargin + static_cast<qreal>(index) * kYGap;
                node.position = QPointF(x, y);
            }
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
        case RoleRole:
            return node.role;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> DerivadasGraphModel::roleNames() const {
        return {
            {SsaRole, "ssa"},     {IsTargetRole, "isTarget"}, {PositionRole, "nodeCenter"},
            {LabelRole, "label"}, {RoleRole, "role"},
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

    QString DerivadasGraphModel::svg() const {
        const qreal width = std::max<qreal>(graphWidth_, kNodeWidth + 2 * kMargin);
        const qreal height = std::max<qreal>(graphHeight_, kNodeHeight + 2 * kMargin);
        QString result =
            QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" "
                           "viewBox=\"0 0 %1 %2\">\n"
                           "  <rect width=\"100%\" height=\"100%\" fill=\"#2f2a27\"/>\n")
                .arg(width)
                .arg(height);

        std::unordered_map<std::string, QPointF> centerBySsa;
        centerBySsa.reserve(nodes_.size());
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
            const bool leftToRight = fromIt->second.x() <= toIt->second.x();
            const qreal fromX =
                fromIt->second.x() + (leftToRight ? kNodeWidth / 2.0 : -kNodeWidth / 2.0);
            const qreal toX =
                toIt->second.x() + (leftToRight ? -kNodeWidth / 2.0 : kNodeWidth / 2.0);
            const qreal routeX = routeXBetween(fromX, toX);
            const auto laneY = lowerLaneY(fromIt->second.y(), toIt->second.y());
            const QString path = laneY.has_value()
                                     ? QStringLiteral("M %1 %2 H %3 V %4 H %5 V %6 H %7")
                                           .arg(fromX)
                                           .arg(fromIt->second.y())
                                           .arg(sourceExitX(fromX, toX))
                                           .arg(laneY.value())
                                           .arg(targetApproachX(fromX, toX))
                                           .arg(toIt->second.y())
                                           .arg(toX)
                                     : QStringLiteral("M %1 %2 H %3 V %4 H %5")
                                           .arg(fromX)
                                           .arg(fromIt->second.y())
                                           .arg(routeX)
                                           .arg(toIt->second.y())
                                           .arg(toX);
            result +=
                QStringLiteral("  <path d=\"%1\" fill=\"none\" stroke=\"#8a8179\" "
                               "stroke-width=\"1.5\"%2/>\n")
                    .arg(path)
                    .arg(edge.dashed ? QStringLiteral(" stroke-dasharray=\"6 4\"") : QString{});
            const qreal direction = leftToRight ? 1.0 : -1.0;
            const QString arrow = QStringLiteral("M %1 %2 L %3 %4 M %1 %2 L %3 %5")
                                      .arg(toX)
                                      .arg(toIt->second.y())
                                      .arg(toX - direction * 5.0)
                                      .arg(toIt->second.y() - 4.0)
                                      .arg(toIt->second.y() + 4.0);
            result +=
                QStringLiteral("  <path d=\"%1\" fill=\"none\" stroke=\"#8a8179\" "
                               "stroke-width=\"1.5\"%2/>\n")
                    .arg(arrow)
                    .arg(edge.dashed ? QStringLiteral(" stroke-dasharray=\"6 4\"") : QString{});
        }

        for (const auto& node : nodes_) {
            const QString fill =
                node.isTarget ? QStringLiteral("#ffbf00") : QStringLiteral("#151a1a");
            const QString stroke =
                node.isTarget ? QStringLiteral("#4a3a00") : QStringLiteral("#7b6f66");
            const QString textColor =
                node.isTarget ? QStringLiteral("#1a1400") : QStringLiteral("#f5deb3");
            result +=
                QStringLiteral("  <rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"7\" "
                               "fill=\"%5\" stroke=\"%6\" stroke-width=\"1.5\"/>\n")
                    .arg(node.position.x())
                    .arg(node.position.y())
                    .arg(kNodeWidth)
                    .arg(kNodeHeight)
                    .arg(fill, stroke);
            result +=
                QStringLiteral("  <text x=\"%1\" y=\"%2\" text-anchor=\"middle\" "
                               "font-family=\"sans-serif\" font-size=\"14\" font-weight=\"700\" "
                               "fill=\"%3\">%4</text>\n")
                    .arg(node.position.x() + kNodeWidth / 2.0)
                    .arg(node.position.y() + 22)
                    .arg(textColor, svgEscaped(node.ssa));
            if (!node.status.isEmpty()) {
                result += QStringLiteral(
                              "  <text x=\"%1\" y=\"%2\" text-anchor=\"middle\" "
                              "font-family=\"sans-serif\" font-size=\"12\" font-weight=\"700\" "
                              "fill=\"%3\">%4</text>\n")
                              .arg(node.position.x() + kNodeWidth / 2.0)
                              .arg(node.position.y() + 40)
                              .arg(textColor, svgEscaped(node.status));
            }
        }

        result += QStringLiteral("</svg>\n");
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
            entry.insert(QStringLiteral("from"), edge.from);
            entry.insert(QStringLiteral("to"), edge.to);
            const bool leftToRight = fromIt->second.x() <= toIt->second.x();
            entry.insert(QStringLiteral("fromX"),
                         fromIt->second.x() + (leftToRight ? kNodeWidth / 2.0 : -kNodeWidth / 2.0));
            entry.insert(QStringLiteral("fromY"), fromIt->second.y());
            entry.insert(QStringLiteral("toX"),
                         toIt->second.x() + (leftToRight ? -kNodeWidth / 2.0 : kNodeWidth / 2.0));
            entry.insert(QStringLiteral("toY"), toIt->second.y());
            const qreal fromX = entry.value(QStringLiteral("fromX")).toReal();
            const qreal fromY = entry.value(QStringLiteral("fromY")).toReal();
            const qreal toX = entry.value(QStringLiteral("toX")).toReal();
            const qreal toY = entry.value(QStringLiteral("toY")).toReal();
            const auto laneY = lowerLaneY(fromY, toY);
            if (laneY.has_value()) {
                entry.insert(QStringLiteral("routeX"), sourceExitX(fromX, toX));
                entry.insert(QStringLiteral("routeY"), laneY.value());
                entry.insert(QStringLiteral("approachX"), targetApproachX(fromX, toX));
            } else {
                entry.insert(QStringLiteral("routeX"), routeXBetween(fromX, toX));
            }
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

    QString DerivadasGraphModel::nodeRole(const int index) const {
        if (index < 0 || index >= static_cast<int>(nodes_.size())) {
            return {};
        }
        return nodes_[index].role;
    }

} // namespace ssa::presentation
