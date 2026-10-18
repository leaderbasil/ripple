#include "ripple/app.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "ripple/analyzer.hpp"
#include "ripple/graph.hpp"
#include "ripple/scanner.hpp"
#include "ripple/tui.hpp"

namespace fs = std::filesystem;

namespace tui = ripple::tui;

using ripple::Analyzer;
using ripple::SourceScanner;

namespace ripple {

namespace {

/// Lists files sorted by name (used for the interactive picker).
std::vector<std::string> listFiles(const SourceScanner& scanner) {
    const auto scan = scanner.scan();
    std::vector<std::string> files = scan.files;
    std::sort(files.begin(), files.end());
    return files;
}

void showWhatIf(const DependencyGraph& graph, const std::string& file) {
    tui::renderWhatIf(graph, file);
    tui::renderTree(graph, file, graph.affectedBy({file}), false);
}

void clearScreen() {
    tui::ansi::clear();
    tui::ansi::moveHome();
}

void pauseForReturn() {
    std::printf("\n[enter to return, q to quit] ");
    std::fflush(stdout);
    if (tui::readKey() == "q") std::exit(0);
}

/// The in-app explanation screen: the five RIPPLE stages, with concrete
/// output so the reader sees each stage do its job.
void cmdHowItWorks() {
    clearScreen();
    std::printf(
        "THE RIPPLE ALGORITHM - 5 stages\n"
        "===============================\n"
        "Metaphor: an edited file is a stone dropped in water;\n"
        "only the files it is CONNECTED to feel the wave.\n\n"
        " 1. SCAN        walk the folder, read every  #include \"...\"\n"
        "                and turn them into graph edges.\n"
        "                new/renamed/deleted files are picked up here,\n"
        "                a zero-config rebuild map.\n\n"
        " 2. FINGERPRINT hash every file's bytes (FNV-1a 64-bit) and\n"
        "                compare with the previous run's baseline.\n"
        "                mtime+size unchanged -> skip the read,\n"
        "                truly edited files -> marked as changed.\n\n"
        " 3. RIPPLE      BFS outwards from the changed files along\n"
        "                \"who includes me\" edges = the exact minimal\n"
        "                set that MUST be rebuilt. Everyone else:\n"
        "                untouched, not recompiled, not re-read.\n\n"
        " 4. ORDER       Kahn's topological sort on that set:\n"
        "                files nothing depends on build first,\n"
        "                their dependents follow.\n"
        "                cycles are reported instead of hanging.\n\n"
        " 5. SAVE        the new map+hashes are written back\n"
        "                atomically, so the next run starts warmer.\n\n"
        "GOAL: edit 1 file out of 10 000 -> this engine touches ~3 stat\n"
        "       calls, 1 hash and a few hundred graph edges,\n"
        "       instead of recompiling 10 000 files.\n\n"
        "Example (demo_app, editing user_model.h):\n"
        "   changed:  user_model.h\n"
        "   affected: home.cpp, user_model.cpp   (main.cpp NOT affected:\n"
        "              it does not include user_model.h)\n"
        "   order:    user_model.h -> home.cpp -> user_model.cpp\n"
        "   speedup:  9 files kept, only 2 recompiled = ~78%% of the work saved\n\n");
    pauseForReturn();
}

}  // namespace

int App::printUsage() {
    std::printf(
        "ripple - the RIPPLE engine (incremental build impact analyzer)\n"
        "\n"
        "usage:\n"
        "  ripple                          interactive menu (default dir '.')\n"
        "  ripple scan [dir]               discovery + edge overview\n"
        "  ripple status [dir]             compare vs baseline, print affected + order\n"
        "  ripple what-if [dir] [file]     simulated change impact (file may be picked)\n"
        "  ripple init [dir]               (re)capture baseline\n"
        "  ripple help                     this text\n"
        "\n"
        "state file: .ripple/baseline.bin inside [dir]\n"
        "\n"
        "examples:\n"
        "  ripple samples/demo_app\n"
        "  ripple status samples/demo_app\n"
        "  ripple what-if samples/demo_app user_model.h\n");
    return 0;
}

int App::cmdScan(const Options& opt) {
    const SourceScanner scanner(opt.root);
    const auto scan = scanner.scan();

    std::printf("root: %s\n", opt.root.c_str());
    std::printf("files discovered: %zu\n", scan.files.size());
    std::printf("edges: %zu\n", scan.graph.edgeCount());

    if (!scan.unresolved.empty()) {
        std::printf("unresolved includes (%zu): first few:\n", scan.unresolved.size());
        for (std::size_t i = 0; i < scan.unresolved.size() && i < 5; ++i) {
            std::printf("  ?? %s\n", scan.unresolved[i].c_str());
        }
    }

    std::printf("\ndependency edges (a -> who depends on a):\n");
    for (const auto& file : scan.files) {
        const auto& deps = scan.graph.dependentsOf(file);
        if (deps.empty()) continue;
        std::printf("%-24s ->", file.c_str());
        for (const auto& d : deps) {
            std::printf(" %s", d.c_str());
        }
        std::printf("\n");
    }
    return 0;
}

int App::cmdStatus(Options& opt) {
    const fs::path baselinePath = fs::path(opt.root) / opt.baseline;
    opt.baseline = baselinePath.string();

    if (!fs::exists(baselinePath)) {
        std::printf("no baseline yet under '%s' - run: ripple init %s\n",
                    opt.root.c_str(), opt.root.c_str());
        return 2;
    }

    Analyzer analyzer({opt.root, opt.baseline});
    const auto result = analyzer.run(Analyzer::Mode::Compare);
    printReport(result);

    if (result.changed.empty() && result.affected.empty()) {
        std::printf("\nNothing to rebuild - up to date.\n");
        return 0;
    }
    return 0;
}

int App::cmdInit(Options& opt) {
    const fs::path baselinePath = fs::path(opt.root) / opt.baseline;
    opt.baseline = baselinePath.string();

    Analyzer analyzer({opt.root, opt.baseline});
    const auto result = analyzer.run(Analyzer::Mode::Capture);
    std::printf("baseline captured: %zu files, %zu edges -> %s\n",
                result.filesScanned, result.edges, opt.baseline.c_str());

    const SourceScanner scanner(opt.root);
    const auto scan = scanner.scan();
    if (!scan.unresolved.empty()) {
        std::printf("note: %zu include(s) did not resolve locally. example:\n",
                    scan.unresolved.size());
        for (std::size_t i = 0; i < 3 && i < scan.unresolved.size(); ++i) {
            std::printf("  ?? %s\n", scan.unresolved[i].c_str());
        }
    }
    return 0;
}

int App::cmdWhatIf(const Options& opt, std::string file) {
    const SourceScanner scanner(opt.root);
    const auto scan = scanner.scan();

    if (file.empty()) {
        auto files = listFiles(scanner);
        file = tui::pickFile(files, "what-if - pick a file to edit (simulation)");
        if (file.empty()) {
            std::printf("(cancelled)\n");
            return 0;
        }
    }

    if (!scan.graph.hasNode(file)) {
        std::printf("unknown or unreachable file: '%s'\n", file.c_str());
        return 2;
    }

    showWhatIf(scan.graph, file);
    return 0;
}

int App::cmdMenu(Options& opt) {
    tui::ansi::enableVirtualTerminal();

    const SourceScanner scanner(opt.root);
    for (;;) {
        clearScreen();

        std::printf("  R I P P L E   -   incremental build impact\n");
        std::printf("  root: %s\n", opt.root.c_str());
        std::printf("  -------------------------------------------\n");
        std::printf("  [1|s] scan       what the graph looks like\n");
        std::printf("  [2|d] status     what changed + minimal rebuild plan\n");
        std::printf("  [3|w] what-if    pick a file, simulate the edit\n");
        std::printf("  [4|b] baseline   recapture (.ripple/baseline.bin)\n");
        std::printf("  [5|h] how it works   the RIPPLE algorithm, 5 stages\n");
        std::printf("  [q|esc] quit\n");
        std::printf("  > ");
        std::fflush(stdout);

        const std::string key = tui::readKey();
        if (key == "q" || key == "esc") return 0;

        const bool isScan = (key == "1" || key == "s");
        const bool isStatus = (key == "2" || key == "d");
        const bool isBaseline = (key == "4" || key == "b");
        const bool isAbout = (key == "5" || key == "h");

        if (isAbout) {
            cmdHowItWorks();
            continue;
        }

        if (isScan || isStatus || isBaseline) {
            clearScreen();
            if (isScan) {
                cmdScan(opt);
            } else if (isStatus) {
                cmdStatus(opt);
            } else {
                cmdInit(opt);
            }
            pauseForReturn();
        } else if (key == "3" || key == "w") {
            auto files = listFiles(scanner);
            const std::string pick =
                tui::pickFile(files, "what-if - pick a file to edit (simulation)");
            if (!pick.empty()) {
                clearScreen();
                const auto scanNow = scanner.scan();
                showWhatIf(scanNow.graph, pick);
                pauseForReturn();
            }
        }
    }
}

int App::run(int argc, char** argv) {
    Options opt;
    std::string command;
    std::string fileArg;
    bool haveDir = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "help" || arg == "--help" || arg == "-h") {
            return printUsage();
        }
        if (arg == "menu" || arg == "scan" || arg == "status" ||
            arg == "what-if" || arg == "init") {
            command = arg;
            continue;
        }
        if (!haveDir) {
            opt.root = arg;
            haveDir = true;
        } else {
            fileArg = arg;
        }
    }
    if (command.empty()) command = "menu";

    if (command == "menu") return cmdMenu(opt);
    if (command == "help") return printUsage();
    if (command == "scan") return cmdScan(opt);
    if (command == "status") return cmdStatus(opt);
    if (command == "init") return cmdInit(opt);
    if (command == "what-if") return cmdWhatIf(opt, fileArg);

    std::printf("unknown command '%s' - try 'ripple help'\n", command.c_str());
    return 2;
}

}  // namespace ripple
