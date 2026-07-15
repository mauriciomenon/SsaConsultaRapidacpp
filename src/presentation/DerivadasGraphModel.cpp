#include "presentation/DerivadasGraphModel.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace ssa::presentation {

    namespace {

        // Mirrors gui/ssa/details_dialog_constants.py
        constexpr qreal kNodeWidth = 118;
        constexpr qreal kNodeHeight = 48;
        constexpr qreal kXGap = 170;
        constexpr qreal kYGap = kNodeHeight + 44;
        constexpr int kMaxChildrenPerRow = 4;
        constexpr qreal kChildRowGap = kNodeHeight + 28;
        constexpr qreal kMargin = 8;
        constexpr qreal kVerticalLayoutWidthThreshold = 900;

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
            return (std::min)(kMaxChildrenPerRow, static_cast<int>(childCount));
        }

        qreal routeXBetween(const qreal fromX, const qreal toX) {
            const qreal distance = std::abs(toX - fromX);
            const qreal offset = (std::min<qreal>)(28.0, distance / 2.0);
            return toX + (fromX <= toX ? -offset : offset);
        }

    } // namespace

    DerivadasGraphModel::DerivadasGraphModel(QObject* parent) : QObject(parent) {}

    void DerivadasGraphModel::setViewportSize(const qreal width, const qreal height) {
        const bool nextVertical = width > 0 && (width < kVerticalLayoutWidthThreshold ||
                                                (height > 0 && height > width * 1.2));
        if (nextVertical == verticalLayout_ || nodes_.empty()) {
            return;
        }
        verticalLayout_ = nextVertical;
        relayout();
        emit graphChanged();
    }

    void DerivadasGraphModel::buildFromRelations(const QString& target,
                                                 const QVariantList& relations) {
        nodes_.clear();
        edges_.clear();
        ancestorSsa_.clear();
        childSsa_.clear();
        target_ = target.trimmed();

        if (target_.isEmpty()) {
            graphWidth_ = 0;
            graphHeight_ = 0;
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

        const auto appendNode = [&](const QString& ssa, const QString& role) {
            if (ssa.isEmpty()) {
                return;
            }
            const auto key = ssa.toStdString();
            if (seenIndex.find(key) != seenIndex.end()) {
                return;
            }
            seenIndex.emplace(key, static_cast<int>(orderedSsa.size()));
            orderedSsa.push_back(ssa);
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

        for (int index = 0; index < static_cast<int>(ancestors.size()); ++index) {
            appendNode(ancestors[index], QStringLiteral("parent"));
        }
        appendNode(target_, QStringLiteral("current"));

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
                appendNode(ssa, isRelated ? QStringLiteral("related") : QStringLiteral("child"));
            }
        }

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

        ancestorSsa_ = ancestors;
        childSsa_.reserve(children.size());
        for (const auto& [child, dashed] : children) {
            childSsa_.push_back(child);
        }

        relayout();

        emit graphChanged();
    }

    void DerivadasGraphModel::relayout() {
        if (nodes_.empty()) {
            graphWidth_ = 0;
            graphHeight_ = 0;
            return;
        }

        std::unordered_map<std::string, QPointF> positionBySsa;
        positionBySsa.reserve(nodes_.size());
        const int childColumns = childColumnCount(childSsa_.size());
        const int childRows =
            childColumns == 0
                ? 0
                : static_cast<int>((childSsa_.size() + childColumns - 1) / childColumns);
        QPointF targetPosition;
        if (verticalLayout_) {
            const qreal targetX =
                childColumns > 0 ? static_cast<qreal>(childColumns - 1) * kXGap / 2.0 : 0;
            targetPosition = QPointF(targetX, static_cast<qreal>(ancestorSsa_.size()) * kYGap);
            positionBySsa.emplace(target_.toStdString(), targetPosition);
            for (std::size_t index = 0; index < ancestorSsa_.size(); ++index) {
                positionBySsa.emplace(ancestorSsa_[index].toStdString(),
                                      QPointF(targetX, static_cast<qreal>(index) * kYGap));
            }
            for (std::size_t index = 0; index < childSsa_.size(); ++index) {
                const int row = childColumns > 0 ? static_cast<int>(index) / childColumns : 0;
                const int column = childColumns > 0 ? static_cast<int>(index) % childColumns : 0;
                positionBySsa.emplace(
                    childSsa_[index].toStdString(),
                    QPointF(static_cast<qreal>(column) * kXGap,
                            targetPosition.y() + kYGap + static_cast<qreal>(row) * kChildRowGap));
            }
        } else {
            const qreal targetX = static_cast<qreal>(ancestorSsa_.size()) * kXGap;
            const qreal targetY =
                childRows > 1 ? static_cast<qreal>(childRows - 1) * kChildRowGap / 2.0 : 0;
            targetPosition = QPointF(targetX, targetY);
            positionBySsa.emplace(target_.toStdString(), targetPosition);
            for (std::size_t index = 0; index < ancestorSsa_.size(); ++index) {
                positionBySsa.emplace(ancestorSsa_[index].toStdString(),
                                      QPointF(static_cast<qreal>(index) * kXGap, targetY));
            }
            for (std::size_t index = 0; index < childSsa_.size(); ++index) {
                const int row = childColumns > 0 ? static_cast<int>(index) / childColumns : 0;
                const int column = childColumns > 0 ? static_cast<int>(index) % childColumns : 0;
                positionBySsa.emplace(
                    childSsa_[index].toStdString(),
                    QPointF(targetX + kXGap + static_cast<qreal>(column) * kXGap,
                            targetY + kYGap + static_cast<qreal>(row) * kChildRowGap));
            }
        }

        for (auto& node : nodes_) {
            const auto positionIt = positionBySsa.find(node.ssa.toStdString());
            if (positionIt != positionBySsa.end()) {
                node.position = positionIt->second;
            }
        }

        qreal minX = nodes_.front().position.x();
        qreal maxX = minX + kNodeWidth;
        qreal minY = nodes_.front().position.y();
        qreal maxY = minY + kNodeHeight;
        QPointF actualTarget = targetPosition;
        for (const auto& node : nodes_) {
            minX = (std::min)(minX, node.position.x());
            maxX = (std::max)(maxX, node.position.x() + kNodeWidth);
            minY = (std::min)(minY, node.position.y());
            maxY = (std::max)(maxY, node.position.y() + kNodeHeight);
            if (node.isTarget) {
                actualTarget = node.position;
            }
        }

        const qreal targetCenterX = actualTarget.x() + kNodeWidth / 2.0;
        const qreal targetCenterY = actualTarget.y() + kNodeHeight / 2.0;
        const qreal halfWidth = (std::max)(targetCenterX - minX, maxX - targetCenterX);
        const qreal halfHeight = (std::max)(targetCenterY - minY, maxY - targetCenterY);
        const qreal translateX = kMargin + halfWidth - targetCenterX;
        const qreal translateY = kMargin + halfHeight - targetCenterY;
        for (auto& node : nodes_) {
            node.position += QPointF(translateX, translateY);
        }

        graphWidth_ = 2 * halfWidth + 2 * kMargin;
        graphHeight_ = 2 * halfHeight + 2 * kMargin;
    }

    int DerivadasGraphModel::rowCount() const {
        return static_cast<int>(nodes_.size());
    }

    QString DerivadasGraphModel::target() const {
        return target_;
    }

    QString DerivadasGraphModel::summary() const {
        return QStringLiteral("Nos: %1 | Relacoes: %2")
            .arg(static_cast<int>(nodes_.size()))
            .arg(static_cast<int>(edges_.size()));
    }

    QString DerivadasGraphModel::svg() const {
        const qreal width = (std::max<qreal>)(graphWidth_, kNodeWidth + 2 * kMargin);
        const qreal height = (std::max<qreal>)(graphHeight_, kNodeHeight + 2 * kMargin);
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
            const auto dash = edge.dashed ? QStringLiteral(" stroke-dasharray=\"6 4\"") : QString{};
            QString path;
            QString arrow;
            if (verticalLayout_) {
                const qreal fromX = fromIt->second.x();
                const qreal fromY = fromIt->second.y() + kNodeHeight / 2.0;
                const qreal toX = toIt->second.x();
                const qreal toY = toIt->second.y() - kNodeHeight / 2.0;
                const qreal routeY = (fromY + toY) / 2.0;
                path = QStringLiteral("M %1 %2 V %3 H %4 V %5")
                           .arg(fromX)
                           .arg(fromY)
                           .arg(routeY)
                           .arg(toX)
                           .arg(toY);
                const qreal direction = fromY <= toY ? 1.0 : -1.0;
                arrow = QStringLiteral("M %1 %2 L %3 %4 M %1 %2 L %5 %4")
                            .arg(toX)
                            .arg(toY)
                            .arg(toX - 4.0)
                            .arg(toY - direction * 5.0)
                            .arg(toX + 4.0);
            } else {
                const bool leftToRight = fromIt->second.x() <= toIt->second.x();
                const qreal fromX =
                    fromIt->second.x() + (leftToRight ? kNodeWidth / 2.0 : -kNodeWidth / 2.0);
                const qreal toX =
                    toIt->second.x() + (leftToRight ? -kNodeWidth / 2.0 : kNodeWidth / 2.0);
                const qreal routeX = routeXBetween(fromX, toX);
                path = QStringLiteral("M %1 %2 H %3 V %4 H %5")
                           .arg(fromX)
                           .arg(fromIt->second.y())
                           .arg(routeX)
                           .arg(toIt->second.y())
                           .arg(toX);
                const qreal direction = leftToRight ? 1.0 : -1.0;
                arrow = QStringLiteral("M %1 %2 L %3 %4 M %1 %2 L %3 %5")
                            .arg(toX)
                            .arg(toIt->second.y())
                            .arg(toX - direction * 5.0)
                            .arg(toIt->second.y() - 4.0)
                            .arg(toIt->second.y() + 4.0);
            }
            result += QStringLiteral("  <path d=\"%1\" fill=\"none\" stroke=\"#8a8179\" "
                                     "stroke-width=\"1.5\"%2/>\n")
                          .arg(path, dash);
            result += QStringLiteral("  <path d=\"%1\" fill=\"none\" stroke=\"#8a8179\" "
                                     "stroke-width=\"1.5\"%2/>\n")
                          .arg(arrow, dash);
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
                    .arg(node.position.y() + 21)
                    .arg(textColor, svgEscaped(node.ssa));
            if (!node.status.isEmpty()) {
                result += QStringLiteral("  <text x=\"%1\" y=\"%2\" text-anchor=\"middle\" "
                                         "font-family=\"sans-serif\" font-size=\"10\" "
                                         "font-weight=\"600\" fill=\"%3\">%4</text>\n")
                              .arg(node.position.x() + kNodeWidth / 2.0)
                              .arg(node.position.y() + 37)
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

    QString DerivadasGraphModel::orientation() const {
        return verticalLayout_ ? QStringLiteral("vertical") : QStringLiteral("horizontal");
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
            if (verticalLayout_) {
                entry.insert(QStringLiteral("fromX"), fromIt->second.x());
                entry.insert(QStringLiteral("fromY"), fromIt->second.y() + kNodeHeight / 2.0);
                entry.insert(QStringLiteral("toX"), toIt->second.x());
                entry.insert(QStringLiteral("toY"), toIt->second.y() - kNodeHeight / 2.0);
                const qreal fromY = entry.value(QStringLiteral("fromY")).toReal();
                const qreal toY = entry.value(QStringLiteral("toY")).toReal();
                entry.insert(QStringLiteral("routeY"), (fromY + toY) / 2.0);
            } else {
                const bool leftToRight = fromIt->second.x() <= toIt->second.x();
                entry.insert(QStringLiteral("fromX"),
                             fromIt->second.x() +
                                 (leftToRight ? kNodeWidth / 2.0 : -kNodeWidth / 2.0));
                entry.insert(QStringLiteral("fromY"), fromIt->second.y());
                entry.insert(QStringLiteral("toX"),
                             toIt->second.x() +
                                 (leftToRight ? -kNodeWidth / 2.0 : kNodeWidth / 2.0));
                entry.insert(QStringLiteral("toY"), toIt->second.y());
                const qreal fromX = entry.value(QStringLiteral("fromX")).toReal();
                const qreal toX = entry.value(QStringLiteral("toX")).toReal();
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
