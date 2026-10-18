#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ripple {

/// FNV-1a 64-bit - single pass, excellent distribution, ~1 GB/s.
std::uint64_t fnv1a64(std::string_view data);

/// Streams the file from disk and hashes its bytes.
/// On failure sets *ok = false (when provided) and returns 0.
std::uint64_t hashFile(const std::string& path, bool* ok = nullptr);

}  // namespace ripple
