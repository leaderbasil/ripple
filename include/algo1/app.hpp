#pragma once

#include <string>

namespace algo1 {

/// Command-line front end: argument parsing, the five sub-commands and
/// the interactive menu. All screen/interaction logic lives here so the
/// analyzer/graph core stays headless and reusable.
class App {
public:
    struct Options {
        std::string root = ".";         ///< project directory to analyze
        std::string baseline = ".algo1/baseline.bin";  ///< state file
        bool interactive = false;       ///< menu-driven mode flag
        bool colors = true;             ///< ANSI color enable/disable
    };

    int run(int argc, char** argv);

    // Exposed for unit testing; each maps to one CLI verb.
    int cmdScan(const Options& opt);
    int cmdStatus(Options& opt);
    int cmdInit(Options& opt);
    int cmdWhatIf(const Options& opt, std::string file);
    int cmdMenu(Options& opt);

private:
    static int printUsage();
};

}  // namespace algo1
