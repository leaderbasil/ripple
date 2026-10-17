#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "algo1/graph.hpp"

namespace algo1 {

/// High-level engine: one call per run.
class Analyzer {
public:
    struct Options {
        std::string root;         ///< project directory to watch
        std::string baselinePath; ///< state file (e.g. .algo1/state.json-ish)
    };

    enum class Mode {
        Capture,  ///< first run: rebuild baseline, verify graph, no report
        Compare,  ///< subsequent runs: diff vs baseline + reports
    };

    struct Result {
        std::vector<std::string> changed;
        std::vector<std::string> added;
        std::vector<std::string> removed;
        std::unordered_set<std::string> affected;
        std::vector<std::string> buildOrder;  ///< Kahn order (compare mode only)
        std::vector<std::string> cyclic;      ///< unreachable order: cycle members
        std::size_t filesScanned = 0;
        std::size_t edges = 0;
        bool baselineWasStale = false;  ///< file set changed since last run
    };

    explicit Analyzer(Options options);

    /// Scan `root`, diff against baseline, persist the new baseline.
    Result run(Mode mode);

    const DependencyGraph& graph() const { return graph_; }

private:
    Options options_;
    DependencyGraph graph_;
};

/// Pretty console output for a Result (compare mode report).
void printReport(const Analyzer::Result& result);

}  // namespace algo1
