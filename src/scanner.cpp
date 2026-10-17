#include "algo1/scanner.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "algo1/utils.hpp"

namespace fs = std::filesystem;

namespace algo1 {

bool isSourceExtension(const std::string& extension) {
    std::string e = extension;
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char* x : {".c", ".cpp", ".cxx", ".cc", ".c++",
                          ".h", ".hpp", ".hxx", ".hh", ".inl"}) {
        if (e == x) return true;
    }
    return false;
}

SourceScanner::SourceScanner(std::string root)
    : root_(std::move(root)) {}

std::vector<std::string> SourceScanner::discoverSources() const {
    std::vector<std::string> out;
    for (const auto& entry : fs::recursive_directory_iterator(root_)) {
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();

        // Skip anything under a hidden directory (.git, .algo1, ...) and
        // dot-files themselves. One bad path key would poison the baseline.
        bool hidden = false;
        for (const auto& part : p) {
            const std::string name = part.filename().string();
            if (!name.empty() && name[0] == '.') {
                hidden = true;
                break;
            }
        }
        if (hidden) continue;

        std::string rel = fs::relative(p, root_).string();
        for (char& c : rel) {
            c = (c == '\\') ? '/' : c;  // POSIX-normalize keys & display
        }
        out.push_back(std::move(rel));
    }
    std::sort(out.begin(), out.end());
    return out;
}

namespace {

std::string lowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

/// Extracts the quoted relative target of an `#include "..."` directive.
/// Returns "" for system includes or malformed lines. Deliberately
/// conservative: only plain quoted includes participate in the graph.
std::string parseQuotedInclude(const std::string& line) {
    const auto hash = line.find('#');
    if (hash == std::string::npos) return "";

    // # must be the first non-blank character on the line.
    for (std::size_t i = 0; i < hash; ++i) {
        if (!std::isspace(static_cast<unsigned char>(line[i]))) return "";
    }

    const auto inc = line.find("include", hash);
    if (inc != hash + 1) return "";

    const auto q1 = line.find('"', inc);
    if (q1 == std::string::npos) return "";            // <...> system include
    const auto q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";            // unterminated

    return line.substr(q1 + 1, q2 - q1 - 1);
}

}  // namespace

ScanResult SourceScanner::scan() const {
    ScanResult out;
    out.files = discoverSources();

    // Pass 1: register every file as a node (isolated files stay visible).
    for (const auto& f : out.files) out.graph.addOrphan(f);

    // Name lookup over canonical keys: bare "user_model.h" and
    // nested "sub/user_model.h" both resolve case-insensitively.
    std::unordered_map<std::string, std::string> byName;
    byName.reserve(out.files.size() * 2);
    for (const auto& f : out.files) {
        byName.emplace(lowerAscii(f), f);
        const auto slash = f.find_last_of('/');
        if (slash != std::string::npos) {
            byName.emplace(lowerAscii(f.substr(slash + 1)), f);  // first wins
        }
    }

    // Pass 2: resolve quoted includes into graph edges.
    for (const auto& relPath : out.files) {
        std::string text;
        if (!readTextFile((fs::path(root_) / relPath).string(), text)) continue;

        std::istringstream stream{text};
        std::string line;
        while (std::getline(stream, line)) {
            const std::string target = parseQuotedInclude(line);
            if (target.empty()) continue;

            const auto it = byName.find(lowerAscii(target));
            if (it != byName.end()) {
                if (it->second != relPath) {  // ignore self-includes
                    out.graph.addEdge(it->second, relPath);  // dep -> dependent
                }
            } else {
                out.unresolved.push_back('"' + target + "\"  (from " + relPath + ")");
            }
        }
    }

    std::sort(out.unresolved.begin(), out.unresolved.end());
    out.unresolved.erase(std::unique(out.unresolved.begin(), out.unresolved.end()),
                         out.unresolved.end());
    return out;
}

}  // namespace algo1
