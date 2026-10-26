#include "domain/DerivationGraph.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <map>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace ssa::domain::derivation {

    namespace {

        // Edges as integer adjacency over the sorted node names (children and parents
        // deduplicated and sorted, so every traversal is deterministic).
        struct IndexedGraph final {
            std::vector<std::string> names;
            std::vector<std::vector<int>> children;
            std::vector<std::vector<int>> parents;
        };

        IndexedGraph indexGraph(const std::span<const MatrixEdge> edges) {
            IndexedGraph graph;
            std::set<std::string> nodes;
            for (const auto& edge : edges) {
                nodes.insert(edge.parent);
                nodes.insert(edge.child);
            }
            graph.names.assign(nodes.begin(), nodes.end());
            std::unordered_map<std::string_view, int> index;
            index.reserve(graph.names.size());
            for (std::size_t position = 0; position < graph.names.size(); ++position) {
                index.emplace(graph.names[position], static_cast<int>(position));
            }
            graph.children.resize(graph.names.size());
            graph.parents.resize(graph.names.size());
            for (const auto& edge : edges) {
                const int parent = index.at(edge.parent);
                const int child = index.at(edge.child);
                graph.children[static_cast<std::size_t>(parent)].push_back(child);
                graph.parents[static_cast<std::size_t>(child)].push_back(parent);
            }
            for (auto* lists : {&graph.children, &graph.parents}) {
                for (auto& list : *lists) {
                    std::ranges::sort(list);
                    const auto [first, last] = std::ranges::unique(list);
                    list.erase(first, last);
                }
            }
            return graph;
        }

        std::vector<bool> cycleMask(const IndexedGraph& graph) {
            const auto size = graph.names.size();
            // Kosaraju, iterative: finish order on the graph, then components on the reverse.
            std::vector<bool> visited(size, false);
            std::vector<int> order;
            order.reserve(size);
            std::vector<std::pair<int, std::size_t>> stack;
            for (std::size_t start = 0; start < size; ++start) {
                if (visited[start]) {
                    continue;
                }
                visited[start] = true;
                stack.emplace_back(static_cast<int>(start), 0);
                while (!stack.empty()) {
                    auto& [node, next] = stack.back();
                    const auto& children = graph.children[static_cast<std::size_t>(node)];
                    if (next == children.size()) {
                        order.push_back(node);
                        stack.pop_back();
                        continue;
                    }
                    const int child = children[next++];
                    if (!visited[static_cast<std::size_t>(child)]) {
                        visited[static_cast<std::size_t>(child)] = true;
                        stack.emplace_back(child, 0);
                    }
                }
            }
            std::vector<bool> inCycle(size, false);
            visited.assign(size, false);
            std::vector<int> component;
            for (const int start : std::views::reverse(order)) {
                if (visited[static_cast<std::size_t>(start)]) {
                    continue;
                }
                component.clear();
                std::vector<int> pending{start};
                visited[static_cast<std::size_t>(start)] = true;
                while (!pending.empty()) {
                    const int node = pending.back();
                    pending.pop_back();
                    component.push_back(node);
                    for (const int parent : graph.parents[static_cast<std::size_t>(node)]) {
                        if (!visited[static_cast<std::size_t>(parent)]) {
                            visited[static_cast<std::size_t>(parent)] = true;
                            pending.push_back(parent);
                        }
                    }
                }
                if (component.size() > 1) {
                    for (const int node : component) {
                        inCycle[static_cast<std::size_t>(node)] = true;
                    }
                }
            }
            return inCycle;
        }

        std::vector<int> weakComponentSizes(const IndexedGraph& graph) {
            const auto size = graph.names.size();
            std::vector<int> componentSize(size, 0);
            std::vector<bool> seen(size, false);
            std::vector<int> cluster;
            for (std::size_t start = 0; start < size; ++start) {
                if (seen[start]) {
                    continue;
                }
                cluster.clear();
                std::deque<int> queue{static_cast<int>(start)};
                seen[start] = true;
                while (!queue.empty()) {
                    const int node = queue.front();
                    queue.pop_front();
                    cluster.push_back(node);
                    for (const auto* neighbors : {&graph.children[static_cast<std::size_t>(node)],
                                                  &graph.parents[static_cast<std::size_t>(node)]}) {
                        for (const int neighbor : *neighbors) {
                            if (!seen[static_cast<std::size_t>(neighbor)]) {
                                seen[static_cast<std::size_t>(neighbor)] = true;
                                queue.push_back(neighbor);
                            }
                        }
                    }
                }
                for (const int node : cluster) {
                    componentSize[static_cast<std::size_t>(node)] =
                        static_cast<int>(cluster.size());
                }
            }
            return componentSize;
        }

        // json.dumps(value, ensure_ascii=False) for a string: escapes quote, backslash and
        // control characters; every other byte (UTF-8 included) is copied as is.
        void appendJsonString(std::string& out, const std::string_view text) {
            out.push_back('"');
            for (const char raw : text) {
                const auto ch = static_cast<unsigned char>(raw);
                switch (ch) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                case '\b':
                    out += "\\b";
                    break;
                case '\f':
                    out += "\\f";
                    break;
                default:
                    if (ch < 0x20) {
                        std::array<char, 7> escaped{};
                        std::snprintf(escaped.data(), escaped.size(), "\\u%04x", ch);
                        out += escaped.data();
                    } else {
                        out.push_back(raw);
                    }
                }
            }
            out.push_back('"');
        }

    } // namespace

    int sourcePriority(const std::string_view sourceName) {
        if (sourceName == kSourceDbField) {
            return 10;
        }
        if (sourceName == kSourceSheet) {
            return 20;
        }
        return 0;
    }

    std::vector<MatrixEdge> mergeSources(const std::span<const SourceEdge> sources) {
        struct Merged final {
            MatrixEdge edge;
            int bestPriority{-1};
        };
        std::map<std::pair<std::string, std::string>, Merged> merged;
        for (const auto& source : sources) {
            auto& entry = merged[{source.parent, source.child}];
            entry.edge.sourceFlags |= source.sourceFlag;
            const int priority = sourcePriority(source.sourceName);
            if (priority < entry.bestPriority) {
                continue;
            }
            entry.bestPriority = priority;
            if (source.relationType != kRelationUnknown) {
                entry.edge.relationType = source.relationType;
            }
            if (source.label.has_value() && !source.label->empty()) {
                entry.edge.label = source.label;
            }
        }
        std::vector<MatrixEdge> edges;
        edges.reserve(merged.size());
        for (auto& [pair, entry] : merged) {
            entry.edge.parent = pair.first;
            entry.edge.child = pair.second;
            edges.push_back(std::move(entry.edge));
        }
        return edges;
    }

    std::set<std::string> cycleNodes(const std::span<const MatrixEdge> edges) {
        const auto graph = indexGraph(edges);
        const auto mask = cycleMask(graph);
        std::set<std::string> nodes;
        for (std::size_t node = 0; node < mask.size(); ++node) {
            if (mask[node]) {
                nodes.insert(graph.names[node]);
            }
        }
        return nodes;
    }

    Closure buildClosure(const std::span<const MatrixEdge> edges, const int depthCap,
                         const std::stop_token& stopToken) {
        const auto graph = indexGraph(edges);
        const auto size = graph.names.size();
        Closure closure;
        const auto mask = cycleMask(graph);
        for (std::size_t node = 0; node < size; ++node) {
            if (mask[node]) {
                closure.cycleNodes.insert(graph.names[node]);
            }
        }

        std::vector<int> distance(size, -1);
        std::vector<int> longest(size, -1);
        std::vector<std::int64_t> paths(size, 0);
        std::vector<int> reached;
        for (std::size_t ancestor = 0; ancestor < size; ++ancestor) {
            if (graph.children[ancestor].empty()) {
                continue;
            }
            if (stopToken.stop_requested()) {
                throw std::runtime_error("derivadas sync canceled");
            }
            reached.clear();
            const int root = static_cast<int>(ancestor);
            distance[ancestor] = 0;
            paths[ancestor] = 1;
            reached.push_back(root);
            bool truncated = false;
            std::deque<int> queue{root};
            while (!queue.empty()) {
                const int node = queue.front();
                queue.pop_front();
                const int depth = distance[static_cast<std::size_t>(node)];
                const auto& children = graph.children[static_cast<std::size_t>(node)];
                if (depth >= depthCap) {
                    truncated = truncated || !children.empty();
                    continue;
                }
                for (const int child : children) {
                    auto& childDistance = distance[static_cast<std::size_t>(child)];
                    auto& childPaths = paths[static_cast<std::size_t>(child)];
                    if (childDistance < 0) {
                        childDistance = depth + 1;
                        childPaths = paths[static_cast<std::size_t>(node)];
                        reached.push_back(child);
                        queue.push_back(child);
                    } else if (childDistance == depth + 1) {
                        childPaths = std::min(childPaths + paths[static_cast<std::size_t>(node)],
                                              kPathCountCap);
                    }
                }
            }
            closure.truncatedAncestors += truncated ? 1 : 0;

            longest[ancestor] = 0;
            std::deque<int> longestQueue{root};
            while (!longestQueue.empty()) {
                const int node = longestQueue.front();
                longestQueue.pop_front();
                const int depth = longest[static_cast<std::size_t>(node)];
                if (depth >= depthCap) {
                    continue;
                }
                for (const int child : graph.children[static_cast<std::size_t>(node)]) {
                    auto& childLongest = longest[static_cast<std::size_t>(child)];
                    if (childLongest < depth + 1) {
                        childLongest = depth + 1;
                        longestQueue.push_back(child);
                    }
                }
            }

            std::vector<int> descendants(reached.begin() + 1, reached.end());
            std::ranges::sort(descendants);
            for (const int descendant : descendants) {
                const auto at = static_cast<std::size_t>(descendant);
                closure.rows.push_back({graph.names[ancestor], graph.names[at], distance[at],
                                        std::max(longest[at], distance[at]), paths[at]});
            }
            for (const int node : reached) {
                const auto at = static_cast<std::size_t>(node);
                distance[at] = -1;
                longest[at] = -1;
                paths[at] = 0;
            }
        }
        return closure;
    }

    std::vector<SummaryRow> buildSummary(const std::span<const MatrixEdge> edges,
                                         const Closure& closure) {
        const auto graph = indexGraph(edges);
        const auto size = graph.names.size();
        std::unordered_map<std::string_view, int> index;
        index.reserve(size);
        for (std::size_t node = 0; node < size; ++node) {
            index.emplace(graph.names[node], static_cast<int>(node));
        }
        const auto componentSize = weakComponentSizes(graph);

        std::vector<SummaryRow> rows(size);
        for (std::size_t node = 0; node < size; ++node) {
            auto& row = rows[node];
            row.ssa = graph.names[node];
            row.directParents = static_cast<int>(graph.parents[node].size());
            row.directChildren = static_cast<int>(graph.children[node].size());
            row.componentSize = componentSize[node];
            row.hasCycle = closure.cycleNodes.contains(row.ssa);
            if (graph.parents[node].empty()) {
                row.levelFromRootMin = 0;
                row.levelFromRootMax = 0;
            }
        }
        for (const auto& closureRow : closure.rows) {
            const auto ancestor = static_cast<std::size_t>(index.at(closureRow.ancestor));
            const auto descendant = static_cast<std::size_t>(index.at(closureRow.descendant));
            ++rows[ancestor].descendants;
            ++rows[descendant].ancestors;
            rows[ancestor].levelsBelowMax =
                std::max(rows[ancestor].levelsBelowMax, closureRow.maxDistance);
            if (graph.parents[ancestor].empty()) {
                auto& target = rows[descendant];
                target.levelFromRootMin =
                    std::min(target.levelFromRootMin.value_or(closureRow.minDistance),
                             closureRow.minDistance);
                target.levelFromRootMax =
                    std::max(target.levelFromRootMax.value_or(closureRow.maxDistance),
                             closureRow.maxDistance);
            }
        }
        for (auto& row : rows) {
            row.levelsAboveMax = row.levelFromRootMax.value_or(0);
        }
        return rows;
    }

    Reconciliation reconcile(const std::set<std::string>& knownSsas,
                             const std::span<const MatrixEdge> edges,
                             const std::span<const SourceEdge> sources) {
        Reconciliation result;
        std::map<std::string, std::set<std::string>> parentsByChild;
        std::set<std::string> orphanParents;
        std::set<std::string> orphanChildren;
        for (const auto& edge : edges) {
            parentsByChild[edge.child].insert(edge.parent);
            if (!knownSsas.contains(edge.parent)) {
                orphanParents.insert(edge.parent);
            }
            if (!knownSsas.contains(edge.child)) {
                orphanChildren.insert(edge.child);
            }
        }
        result.multiparentChildren = static_cast<std::size_t>(std::ranges::count_if(
            parentsByChild, [](const auto& entry) { return entry.second.size() > 1; }));
        result.orphanParents = orphanParents.size();
        result.orphanChildren = orphanChildren.size();

        std::map<std::string, std::set<std::string>> dbParents;
        std::map<std::string, std::set<std::string>> sheetParents;
        for (const auto& source : sources) {
            if (source.sourceName == kSourceDbField) {
                dbParents[source.child].insert(source.parent);
            } else if (source.sourceName == kSourceSheet) {
                sheetParents[source.child].insert(source.parent);
            }
        }
        for (const auto& [child, parents] : dbParents) {
            const auto sheet = sheetParents.find(child);
            if (sheet != sheetParents.end() && sheet->second != parents) {
                ++result.dbVsSheetConflicts;
            }
        }
        result.cycleNodes = cycleNodes(edges).size();
        return result;
    }

    std::string fingerprintPayload(const std::span<const MatrixEdge> edges,
                                   const FingerprintFormat format) {
        std::vector<const MatrixEdge*> sorted;
        sorted.reserve(edges.size());
        for (const auto& edge : edges) {
            sorted.push_back(&edge);
        }
        std::ranges::sort(sorted, [](const MatrixEdge* left, const MatrixEdge* right) {
            return std::tie(left->parent, left->child) < std::tie(right->parent, right->child);
        });
        std::string payload;
        for (const auto* edge : sorted) {
            payload += edge->parent;
            payload += "->";
            payload += edge->child;
            payload += ':';
            payload += std::to_string(edge->sourceFlags);
            payload += ';';
            if (format == FingerprintFormat::Legacy) {
                continue;
            }
            payload += '[';
            payload += std::to_string(edge->relationType);
            payload += ", ";
            if (edge->label.has_value()) {
                appendJsonString(payload, *edge->label);
            } else {
                payload += "null";
            }
            payload += "];";
        }
        return payload;
    }

} // namespace ssa::domain::derivation
