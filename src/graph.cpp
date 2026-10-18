#include "ripple/graph.hpp"

#include <algorithm>
#include <queue>
#include <utility>

namespace ripple {

const std::vector<std::string> DependencyGraph::kEmpty_;

void DependencyGraph::addOrphan(const std::string& node) {
    dependents_.try_emplace(node);
    dependencies_.try_emplace(node);
}

void DependencyGraph::addEdge(const std::string& dependency, const std::string& dependent) {
    auto& out = dependents_[dependency];
    if (std::find(out.begin(), out.end(), dependent) == out.end()) {
        out.push_back(dependent);
    }
    auto& in = dependencies_[dependent];
    if (std::find(in.begin(), in.end(), dependency) == in.end()) {
        in.push_back(dependency);
    }
    dependents_.try_emplace(dependent);   // ensure both endpoints are nodes
    dependencies_.try_emplace(dependency);
}

const std::vector<std::string>&
DependencyGraph::dependentsOf(const std::string& node) const {
    const auto it = dependents_.find(node);
    return (it != dependents_.end()) ? it->second : kEmpty_;
}

const std::vector<std::string>&
DependencyGraph::dependenciesOf(const std::string& node) const {
    const auto it = dependencies_.find(node);
    return (it != dependencies_.end()) ? it->second : kEmpty_;
}

bool DependencyGraph::hasNode(const std::string& node) const {
    return dependents_.find(node) != dependents_.end();
}

std::vector<std::string> DependencyGraph::nodes() const {
    std::vector<std::string> out;
    out.reserve(dependents_.size());
    for (const auto& kv : dependents_) out.push_back(kv.first);
    return out;
}

std::size_t DependencyGraph::edgeCount() const {
    std::size_t n = 0;
    for (const auto& kv : dependents_) n += kv.second.size();
    return n;
}

std::unordered_set<std::string>
DependencyGraph::affectedBy(const std::vector<std::string>& changedFiles) const {
    std::unordered_set<std::string> affected;
    std::vector<std::string> work;

    for (const auto& file : changedFiles) {
        if (!hasNode(file)) continue;
        if (affected.insert(file).second) {
            work.push_back(file);
        }
    }

    while (!work.empty()) {
        const std::string node = std::move(work.back());
        work.pop_back();
        for (const auto& dep : dependentsOf(node)) {
            if (affected.insert(dep).second) {
                work.push_back(dep);
            }
        }
    }
    return affected;
}

DependencyGraph::BuildOrder
DependencyGraph::buildOrder(const std::unordered_set<std::string>& subset) const {
    BuildOrder out;

    // Deterministic Kahn's algorithm restricted to `subset`, with
    // alphabetical tie-breaking for reproducible output. O(V + E).
    std::unordered_map<std::string, std::size_t> indegree;
    indegree.reserve(subset.size());
    for (const auto& node : subset) indegree.emplace(node, 0);

    for (const auto& node : subset) {
        for (const auto& dep : dependenciesOf(node)) {   // edge dep -> node
            if (subset.count(dep)) ++indegree[node];
        }
    }

    // Min-heap keyed by node name => lexicographically smallest ready node first.
    using Ready = std::priority_queue<
        std::string, std::vector<std::string>, std::greater<std::string>>;
    Ready ready;
    for (const auto& kv : indegree) {
        if (kv.second == 0) ready.push(kv.first);
    }

    while (!ready.empty()) {
        const std::string node = ready.top();
        ready.pop();
        const auto it = indegree.find(node);
        if (it == indegree.end() || it->second != 0) continue;  // stale entry

        out.sequence.push_back(node);
        it->second = SIZE_MAX;  // mark emitted

        for (const auto& dep : dependentsOf(node)) {
            auto dit = indegree.find(dep);
            if (dit != indegree.end() && dit->second != SIZE_MAX) {
                if (--dit->second == 0) ready.push(dep);
            }
        }
    }

    if (out.sequence.size() < subset.size()) {
        // Everything not emitted sits on (or behind) a cycle.
        out.cyclic.reserve(subset.size() - out.sequence.size());
        for (const auto& node : subset) {
            const auto it = indegree.find(node);
            if (it == indegree.end() || it->second != SIZE_MAX) {
                out.cyclic.push_back(node);
            }
        }
        std::sort(out.cyclic.begin(), out.cyclic.end());
    }
    return out;
}

}  // namespace ripple
