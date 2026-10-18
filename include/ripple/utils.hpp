#pragma once

#include <string>

namespace ripple {

/// Reads an entire file into memory. Returns false when the file
/// cannot be opened or read; on failure `out` is left untouched.
bool readTextFile(const std::string& path, std::string& out);

/// Writes (overwrites) a text file. Returns false on failure.
bool writeTextFile(const std::string& path, const std::string& content);

}  // namespace ripple
