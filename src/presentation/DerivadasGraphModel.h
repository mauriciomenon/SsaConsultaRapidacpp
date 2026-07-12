#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <optional>
#include <unordered_map>
#include <vector>

namespace ssa::presentation {

    // Layout model for the derivadas relation graph, mirroring the Python
    // details_graph_renderer: nodes are SSAs, edges connect parent->child, the
    // target SSA is highlighted. Positions use a depth/index grid layout.
    class DerivadasGraphModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString target READ target NOTIFY graphChanged)
        Q_PROPERTY(QString summary READ summary NOTIFY graphChanged)
        Q_PROPERTY(QString svg READ svg NOTIFY graphChanged)
        Q_PROPERTY(qreal graphWidth READ graphWidth NOTIFY graphChanged)
        Q_PROPERTY(qreal graphHeight READ graphHeight NOTIFY graphChanged)
        Q_PROPERTY(int nodeCount READ rowCount NOTIFY graphChanged)

      public:
        explicit DerivadasGraphModel(QObject* parent = nullptr);

        // Target stays fixed; edges are derived from the relation list passed in.
        void buildFromRelations(const QString& target, const QVariantList& relations);

        Q_INVOKABLE [[nodiscard]] int rowCount() const;

        [[nodiscard]] QString target() const;
        [[nodiscard]] QString summary() const;
        [[nodiscard]] QString svg() const;
        [[nodiscard]] qreal graphWidth() const;
        [[nodiscard]] qreal graphHeight() const;

        // Edge list for the QML Canvas to draw: each entry is {from, to, dashed}.
        Q_INVOKABLE [[nodiscard]] QVariantList edges() const;
        Q_INVOKABLE [[nodiscard]] QPointF nodeCenter(int index) const;
        Q_INVOKABLE [[nodiscard]] QString nodeSsa(int index) const;
        Q_INVOKABLE [[nodiscard]] QString nodeStatus(int index) const;
        Q_INVOKABLE [[nodiscard]] bool nodeIsTarget(int index) const;
        Q_INVOKABLE [[nodiscard]] QString nodeRole(int index) const;

      signals:
        void graphChanged();

      private:
        struct GraphNode {
            QString ssa;
            QString status;
            QString role;
            QPointF position;
            bool isTarget = false;
        };
        struct GraphEdge {
            QString from;
            QString to;
            bool dashed = false;
        };

        std::vector<GraphNode> nodes_;
        std::vector<GraphEdge> edges_;
        QString target_;
        qreal graphWidth_ = 0;
        qreal graphHeight_ = 0;
    };

} // namespace ssa::presentation
