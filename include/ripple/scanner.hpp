#pragma once

#include <string>
#include <vector>

#include "ripple/graph.hpp"

namespace ripple {

struct ScanResult {
    std::vector<std::string> files;  ///< recognized sources, relative to root, sorted
    DependencyGraph graph;           ///< include-derived dependency graph
    std::vector<std::string> unresolved;  ///< `"..."` includes pointing to nothing
};

/// Walks a directory tree and derives the dependency graph from
/// `#include "..."` directives. System includes (`<...>`) are ignored.
class SourceScanner {
public:
    explicit SourceScanner(std::string root);

    ScanResult scan() const;

    const std::string& root() const { return root_; }

private:
    std::vector<std::string> discoverSources() const;

    std::string root_;
};

/// True when `extension` (any case) is a recognized C/C++ source or header.
bool isSourceExtension(const std::string& extension);

}  // namespace ripple
