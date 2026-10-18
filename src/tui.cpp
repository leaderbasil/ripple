#include "ripple/tui.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#endif

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ripple/graph.hpp"

namespace ripple {
namespace tui {

namespace ansi {

#ifdef _WIN32
namespace {
bool gVtEnabled = false;
}
#endif

void enableVirtualTerminal() {
#ifdef _WIN32
    const HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        gVtEnabled = SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    }
    // Box-drawing text arrives as UTF-8 bytes; without this codepage the
    // classic conhost prints mojibake instead of the intended glyphs.
    SetConsoleOutputCP(65001);
#endif
}

void clear() { std::fputs("\033[2J", stdout); }
void moveHome() { std::fputs("\033[H", stdout); }
void hideCursor() { std::fputs("\033[?25l", stdout); }
void showCursor() { std::fputs("\033[?25h", stdout); }
void dim() { std::fputs("\033[90m", stdout); }
void bright() { std::fputs("\033[1m", stdout); }
void red() { std::fputs("\033[31m", stdout); }
void green() { std::fputs("\033[32m", stdout); }
void yellow() { std::fputs("\033[33m", stdout); }
void cyan() { std::fputs("\033[36m", stdout); }
void reset() { std::fputs("\033[0m", stdout); }

}  // namespace ansi

namespace {

#ifdef _WIN32
// EOF-safe: when stdin is redirected/piped and runs dry, quit instead of
// spinning forever in a menu loop.
std::string mapWinKey(int ch) {
    if (ch == -1 || ch == EOF) return "q";
    switch (ch) {
        case 0xE0:  // extended prefix followed by real key
        case 0x00: {
            const int ext = _getch();
            switch (ext) {
                case 72: return "up";
                case 75: return "left";
                case 77: return "right";
                case 80: return "down";
                default: return "any";
            }
        }
        case '\r':
        case '\n': return "enter";
        case 8: return "esc";
        case 27: return "esc";
        default:
            if (ch >= 32 && ch < 127) {
                return std::string(1, static_cast<char>(ch));
            }
            return "any";
    }
}
#else
// termios raw mode window for one keypress read.
std::string mapPosixKey() {
    termios oldt{};
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return "any";
    termios raw = oldt;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    const int ch = std::fgetc(stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (ch == EOF) return "q";

    if (ch == '\r' || ch == '\n') return "enter";
    if (ch == 27) {  // ESC [ A/B/C/D sequences
        if (std::fgetc(stdin) == '[') {
            switch (std::fgetc(stdin)) {
                case 'A': return "up";
                case 'B': return "down";
                case 'D': return "left";
                case 'C': return "right";
                default: return "any";
            }
        }
        return "esc";
    }
    if (ch >= 32 && ch < 127) return std::string(1, static_cast<char>(ch));
    return "any";
}
#endif

std::string padded(const std::string& text, std::size_t width) {
    std::string out = text;
    if (out.size() < width) out.append(width - out.size(), ' ');
    return out;
}

void drawList(const std::vector<std::string>& items, std::size_t cursor) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i == cursor) {
            std::fputs("\033[7m ", stdout);  // reverse-video marker
            std::fputs(items[i].c_str(), stdout);
            ansi::reset();
            std::fputs(" ", stdout);
        } else {
            ansi::dim();
            std::fputs("  ", stdout);
            ansi::reset();
            std::fputs(items[i].c_str(), stdout);
        }
        std::fputs("\n", stdout);
    }
}

}  // namespace

std::string readKey() {
#ifdef _WIN32
    // Interactive console: keys come from the console input buffer.
    // Piped/redirected stdin: _getch() ignores it and blocks forever,
    // so fall back to a portable fgetc() path (used by scripted runs).
    if (_isatty(_fileno(stdin))) {
        const int ch = _getch();
        if (ch == -1 || ch == EOF) return "q";         // redirected stdin, dry pipe
        if (ch == 0x1A) return "q";                    // Ctrl+Z = EOF marker
        return mapWinKey(ch);
    }
    const int ch = std::fgetc(stdin);
    if (ch == EOF) return "q";
    if (ch == '\r' || ch == '\n') return "enter";
    return (ch >= 32 && ch < 127) ? std::string(1, static_cast<char>(ch)) : "any";
#else
    return mapPosixKey();
#endif
}

namespace {

// Depth first recursive printing with cycle guards; also highlights nodes.
      // Direction: dependents - "who must rebuild transitively because of this node".
void printBranch(const DependencyGraph& graph,
                 const std::string& node,
                 const std::string& prefix,
                 std::unordered_set<std::string> path,
                 const std::unordered_set<std::string>& highlight,
                 int depth, int maxDepth) {
    if (depth >= maxDepth) {
        ansi::dim();
        std::printf("%s\n", (prefix + "...").c_str());
        ansi::reset();
        return;
    }

    std::vector<std::string> children = graph.dependentsOf(node);
    std::sort(children.begin(), children.end());
    if (children.empty()) return;

    for (std::size_t i = 0; i < children.size(); ++i) {
        const bool last = (i + 1 == children.size());
        const std::string& child = children[i];

        if (highlight.count(child)) {
            ansi::red();
            std::printf("%s%s %s *affected*\n", prefix.c_str(),
                        last ? "`-- " : "|-- ", child.c_str());
            ansi::reset();
        } else {
            std::printf("%s%s %s\n", prefix.c_str(), last ? "`-- " : "|-- ",
                        child.c_str());
        }

        if (path.insert(child).second) {
            const std::string deeper = prefix + (last ? "    " : "|   ");
            printBranch(graph, child, deeper, path, highlight, depth + 1, maxDepth);
        } else {
            ansi::yellow();
            std::printf("%s%s (shared node - shown above)\n", prefix.c_str(),
                        last ? "    " : "|   ");
            ansi::reset();
        }
    }
}

}  // namespace

void renderTree(const DependencyGraph& graph,
                const std::string& root,
                const std::unordered_set<std::string>& highlight,
                bool onlySubtree) {
    (void)onlySubtree;  // full-tree rendering is the default view

    ansi::bright();
    std::fputs(root.c_str(), stdout);
    ansi::reset();
    if (highlight.count(root)) {
        ansi::red();
        std::fputs("  <= changed", stdout);
        ansi::reset();
    }
    std::fputs("\n", stdout);
    printBranch(graph, root, "", {root}, highlight, 1, 6);
}

void renderStats(const DependencyGraph& graph) {
    const std::size_t nodes = graph.nodes().size();
    const std::size_t edges = graph.edgeCount();
    std::printf("nodes=%zu edges=%zu avg-out=%.2f depth-cap=%d\n", nodes, edges,
                nodes ? double(edges) / double(nodes) : 0.0, 6);
}

void renderWhatIf(const DependencyGraph& graph, const std::string& file) {
    const auto affected = graph.affectedBy({file});
    const auto order = graph.buildOrder(affected);

    std::printf("what-if: changing '%s' requires rebuilding %zu file(s)\n",
                file.c_str(), affected.size());
    std::fputs("order: ", stdout);
    for (std::size_t i = 0; i < order.sequence.size(); ++i) {
        if (i) std::fputs(" -> ", stdout);
        std::fputs(order.sequence[i].c_str(), stdout);
    }
    if (!order.cyclic.empty()) {
        ansi::red();
        std::fputs("  [cycle blocking order!]", stdout);
        ansi::reset();
    }
    std::fputs("\n", stdout);
}

std::string pickFile(const std::vector<std::string>& files, const std::string& title) {
    if (files.empty()) return "";

    std::size_t cursor = 0;
    ansi::hideCursor();
    for (;;) {
        ansi::moveHome();
        ansi::clear();
        {
            ansi::bright();
            std::printf("%s\n", title.c_str());
            ansi::reset();
            ansi::dim();
            std::printf("(up/down or j/k, enter = select, q = quit)\n\n");
            ansi::reset();
        }
        drawList(files, cursor);
        std::fflush(stdout);

        const std::string key = readKey();
        if (key == "up" || key == "k") {
            if (cursor > 0) --cursor;
        } else if (key == "down" || key == "j") {
            if (cursor + 1 < files.size()) ++cursor;
        } else if (key == "enter") {
            ansi::showCursor();
            return files[cursor];
        } else if (key == "q" || key == "esc") {
            ansi::showCursor();
            return "";
        }
    }
}

}  // namespace tui

}  // namespace ripple
