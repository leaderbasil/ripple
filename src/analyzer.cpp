#include "ripple/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "ripple/baseline.hpp"
#include "ripple/hashing.hpp"
#include "ripple/scanner.hpp"

namespace fs = std::filesystem;

namespace ripple {

namespace {

/// File metadata snapshot. mtime+size act as a cheap pre-filter so
/// untouched files skip read+hash entirely on warm runs.
struct StatInfo {
    std::int64_t mtime = 0;
    std::int64_t size = 0;
};

bool statFile(const fs::path& p, StatInfo& out) {
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    if (ec) return false;
    out.mtime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch())
            .count();
    out.size = static_cast<std::int64_t>(fs::file_size(p, ec));
    if (ec) out.size = -1;
    return true;
}

std::vector<std::string> toSortedVector(const std::unordered_set<std::string>& set) {
    std::vector<std::string> out(set.begin(), set.end());
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

Analyzer::Analyzer(Options options)
    : options_(std::move(options)) {}

Analyzer::Result Analyzer::run(const Mode mode) {
    Result result;

    // 1) Dynamic discovery: walk the tree, parse #includes -> graph.
    const SourceScanner scanner(options_.root);
    const ScanResult scan = scanner.scan();
    graph_ = scan.graph;
    result.filesScanned = scan.files.size();
    result.edges = graph_.edgeCount();

    // 2) Load previous state.
    bool healthy = false;
    BaselineData previous = Baseline::load(options_.baselinePath, &healthy);
    BaselineData current;
    current.reserve(scan.files.size());

    // 3) Stat + hash every file once (chunked streaming FNV-1a64).
    std::vector<std::string> changed;
    for (const auto& rel : scan.files) {
        const fs::path abs = fs::path(options_.root) / rel;

        StatInfo stat;
        if (!statFile(abs, stat)) continue;  // vanished mid-scan; skip

        Record rec;
        rec.mtime = stat.mtime;
        rec.size = stat.size;

        const auto prevIt = previous.find(rel);
        const bool known = (prevIt != previous.end());
        const bool fingerprintSame =
            known && prevIt->second.mtime == rec.mtime &&
            prevIt->second.size == rec.size;

        if (fingerprintSame) {
            rec.hash = prevIt->second.hash;  // skip read+hash: warm-run win
        } else {
            bool ok = false;
            rec.hash = hashFile(abs.string(), &ok);
            if (!ok) continue;
        }

        current[rel] = rec;

        if (!known) {
            result.added.push_back(rel);
        } else if (!fingerprintSame && prevIt->second.hash != rec.hash) {
            changed.push_back(rel);  // stat drift confirmed by hash
        }
    }

    // 4) Removals: tracked before, gone now.
    for (const auto& [name, rec] : previous) {
        (void)rec;
        if (current.find(name) == current.end()) result.removed.push_back(name);
    }

    // 5) Impact analysis (compare mode; capture mode only seeds state).
    if (mode == Mode::Compare) {
        result.affected = graph_.affectedBy(changed);
        const auto order = graph_.buildOrder(result.affected);
        result.buildOrder = order.sequence;
        result.cyclic = order.cyclic;
        result.baselineWasStale = !healthy || !result.added.empty() ||
                                  !result.removed.empty();
    }

    result.changed = changed;
    std::sort(result.changed.begin(), result.changed.end());

    // 6) Persist atomically for the next run.
    Baseline::save(options_.baselinePath, current);

    return result;
}

void printReport(const Analyzer::Result& r) {
    auto sorted = [](const std::vector<std::string>& v) {
        std::vector<std::string> out(v);
        std::sort(out.begin(), out.end());
        return out;
    };

    std::printf("== RIPPLE incremental analysis ==\n");
    std::printf("scanned %zu file(s), graph edges %zu\n", r.filesScanned, r.edges);

    if (!r.added.empty()) {
        std::printf("\n[+] new files (%zu)\n", r.added.size());
        for (const auto& f : sorted(r.added)) std::printf("  + %s\n", f.c_str());
    }
    if (!r.removed.empty()) {
        std::printf("\n[-] removed files (%zu)\n", r.removed.size());
        for (const auto& f : sorted(r.removed)) std::printf("  - %s\n", f.c_str());
    }

    std::printf("\n[~] changed (%zu)\n", r.changed.size());
    if (r.changed.empty()) std::printf("  (none)\n");
    for (const auto& f : r.changed) std::printf("  ~ %s\n", f.c_str());

    std::printf("\n[*] affected (%zu)\n", r.affected.size());
    if (r.affected.empty()) {
        std::printf("  (none)\n");
    } else {
        for (const auto& f : toSortedVector(r.affected)) {
            std::printf("  * %s\n", f.c_str());
        }
    }

    if (!r.buildOrder.empty()) {
        std::printf("\n[>] build order (%zu)\n", r.buildOrder.size());
        for (std::size_t i = 0; i < r.buildOrder.size(); ++i) {
            std::printf("  %2zu. %s\n", i + 1, r.buildOrder[i].c_str());
        }
    }
    if (!r.cyclic.empty()) {
        std::printf("\n[!] cyclic (no valid order):\n");
        for (const auto& f : r.cyclic) std::printf("  ! %s\n", f.c_str());
    }
}

}  // namespace ripple
