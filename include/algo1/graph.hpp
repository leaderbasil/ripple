#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace algo1 {

/// Directed dependency graph between source files.
///
/// Edge semantics (single source of truth):
///     addEdge(a, b)  ==>  "b depends on a"  (b `#include`s a)
///
/// Analysis derived from this direction:
///   * When [a] changes, everything transitively reachable through
///     dependents must be rebuilt        -> affectedBy()
///   * A valid build order places [a] before its dependents -> buildOrder()
class DependencyGraph {
public:
    using Adjacency = std::unordered_map<std::string, std::vector<std::string>>;
    using NeighborsFn =
        const std::vector<std::string>& (DependencyGraph::*)(const std::string&) const;

    /// Register a node even when it has no edges (isolated file).
    void addOrphan(const std::string& node);

    /// Add edge `dependency -> dependent`. Duplicate edges are ignored.
    void addEdge(const std::string& dependency, const std::string& dependent);

    const std::vector<std::string>& dependentsOf(const std::string& node) const;
    const std::vector<std::string>& dependenciesOf(const std::string& node) const;
    bool hasNode(const std::string& node) const;

    /// All registered nodes (unsorted).
    std::vector<std::string> nodes() const;

    /// Total number of directed edges.
    std::size_t edgeCount() const;

    /// BFS over `dependents` edges. Result contains the trigger files
    /// themselves plus every transitive dependent.
    std::unordered_set<std::string> affectedBy(
        const std::vector<std::string>& changedFiles) const;

    struct BuildOrder {
        std::vector<std::string> sequence;  ///< Kahn-ordered nodes (or as much as is orderable)
        std::vector<std::string> cyclic;    ///< nodes trapped in a cycle, if any
    };

    /// Kahn's algorithm restricted to `nodes`, with alphabetical
    /// tie-breaking so output is deterministic. O(V + E).
    BuildOrder buildOrder(const std::unordered_set<std::string>& nodes) const;

private:
    Adjacency dependents_;    ///< node -> who includes/depends on it
    Adjacency dependencies_;  ///< node -> what it includes/depends on

    static const std::vector<std::string> kEmpty_;
};

}  // namespace algo1
