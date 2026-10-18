#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "ripple/graph.hpp"

namespace ripple {

/// One per tracked file. Compact: this struct is replayed/rewritten
/// on every run, so it stays POD-like with a stable text encoding.
struct Record {
    std::uint64_t hash = 0;    ///< FNV-1a64 of file bytes at last scan
    std::int64_t  mtime = 0;   ///< write-time, used as a cheap pre-filter
    std::int64_t  size = 0;    ///< byte size, second cheap pre-filter
};

/// `file -> Record` keyed by normalized forward-slash path.
using BaselineData = std::unordered_map<std::string, Record>;

/// Baseline persistence. One atomic rewrite per run (tmp file + rename),
/// so a crash mid-write never corrupts the stored state.
class Baseline {
public:
    /// Load from disk; missing/corrupt file yields an empty baseline and
    /// `*healthy = false`.
    static BaselineData load(const std::string& path, bool* healthy = nullptr);

    /// Persist atomically. Returns false when writing/replacing fails.
    static bool save(const std::string& path, const BaselineData& data);

    /// Replace the whole stored state (used after a scan).
    static void assign(BaselineData& target, BaselineData&& source);
};

}  // namespace ripple
