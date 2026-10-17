#include "algo1/app.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "algo1/analyzer.hpp"
#include "algo1/graph.hpp"
#include "algo1/scanner.hpp"
#include "algo1/tui.hpp"

namespace fs = std::filesystem;

namespace tui = algo1::tui;

using algo1::Analyzer;
using algo1::SourceScanner;

namespace algo1 {

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

}  // namespace

int App::printUsage() {
    std::printf(
        "algo1 — incremental build impact analyzer\n"
        "\n"
        "usage:\n"
        "  algo1                          interactive menu (default dir '.')\n"
        "  algo1 scan [dir]               discovery + edge overview\n"
        "  algo1 status [dir]             compare vs baseline, print affected + order\n"
        "  algo1 what-if [dir] [file]     simulated change impact (file may be picked)\n"
        "  algo1 init [dir]               (re)capture baseline\n"
        "  algo1 help                     this text\n"
        "\n"
        "state file: .algo1/baseline.bin inside [dir]\n"
        "\n"
        "examples:\n"
        "  algo1 samples/demo_app\n"
        "  algo1 status samples/demo_app\n"
        "  algo1 what-if samples/demo_app user_model.h\n");
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
        std::printf("no baseline yet under '%s' — run: algo1 init %s\n",
                    opt.root.c_str(), opt.root.c_str());
        return 2;
    }

    Analyzer analyzer({opt.root, opt.baseline});
    const auto result = analyzer.run(Analyzer::Mode::Compare);
    printReport(result);

    if (result.changed.empty() && result.affected.empty()) {
        std::printf("\nNothing to rebuild — up to date.\n");
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
        file = tui::pickFile(files, "what-if — pick a file to edit (simulation)");
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
        tui::ansi::clear();
        tui::ansi::moveHome();

        std::printf("Algo1  —  root: %s\n", opt.root.c_str());
        std::printf("--------------------------------\n");
        std::printf(" [1] scan       discovery + edge overview\n");
        std::printf(" [2] status     diff vs baseline\n");
        std::printf(" [3] what-if    edit one file (simulated)\n");
        std::printf(" [4] init       recapture baseline\n");
        std::printf(" [q] quit\n");
        std::printf("> ");
        std::fflush(stdout);

        const std::string key = tui::readKey();
        if (key == "q" || key == "esc") return 0;

        if (key == "1") {
            tui::ansi::clear();
            tui::ansi::moveHome();
            cmdScan(opt);
            std::printf("\n[press enter to return] ");
            std::fflush(stdout);
            tui::readKey();
        } else if (key == "2") {
            tui::ansi::clear();
            tui::ansi::moveHome();
            cmdStatus(opt);
            std::printf("\n[press enter to return] ");
            std::fflush(stdout);
            tui::readKey();
        } else if (key == "4") {
            tui::ansi::clear();
            tui::ansi::moveHome();
            cmdInit(opt);
            std::printf("\n[press enter to return] ");
            std::fflush(stdout);
            tui::readKey();
        } else if (key == "3") {
            auto files = listFiles(scanner);
            const std::string pick =
                tui::pickFile(files, "what-if — pick a file to edit (simulation)");
            if (!pick.empty()) {
                tui::ansi::clear();
                tui::ansi::moveHome();
                const auto scanNow = scanner.scan();
                showWhatIf(scanNow.graph, pick);
                std::printf("\n[press enter to return] ");
                std::fflush(stdout);
                tui::readKey();
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

    std::printf("unknown command '%s' — try 'algo1 help'\n", command.c_str());
    return 2;
}

}  // namespace algo1
