#pragma once

#include <string>
#include <vector>

#include "ripple/graph.hpp"

namespace ripple {
namespace tui {

/// Cross-platform single-key read:
///   * Windows: console VT input (arrow keys arrive as vendor sequences)
///   * POSIX:   termios raw mode
/// Returns one of: "up", "down", "left", "right", "enter", "esc",
/// "q", "j", "k", "space", "y", "n", "a", "any".
std::string readKey();

/// ANSI helpers (enabled on Windows via VT processing).
namespace ansi {
void enableVirtualTerminal();
void clear();
void moveHome();
void hideCursor();
void showCursor();
void dim();
void bright();
void red();
void green();
void yellow();
void cyan();
void reset();
}  // namespace ansi

/// Renders the dependency graph as a horizontal tree, depth-first,
/// one branch per line, like:
///
///     main.cpp
///     +-- home.cpp
///     |   `-- user_model.cpp
///     +-- settings.cpp
///     `-- config.cpp
///
/// `highlight` marks nodes (e.g. affected) with a bullet; `start`
/// narrows the view to one file's subtree.
void renderTree(const DependencyGraph& graph,
                const std::string& root,
                const std::unordered_set<std::string>& highlight,
                bool onlySubtree);

/// Compact box summary: nodes, edges, depth.
void renderStats(const DependencyGraph& graph);

/// What-if preview: "if X changes, these N files rebuild, in this order".
void renderWhatIf(const DependencyGraph& graph, const std::string& file);

/// Interactive menu: arrow keys / j-k to move, Enter to pick, q to quit.
/// Returns the chosen file, or "" when the user aborts.
std::string pickFile(const std::vector<std::string>& files,
                     const std::string& title);

}  // namespace tui
}  // namespace ripple
