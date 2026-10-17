#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "algo1/hashing.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace algo1 {

std::uint64_t fnv1a64(std::string_view data) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : data) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::uint64_t hashFile(const std::string& path, bool* ok) {
    // Read in chunks so huge sources don't need a full in-memory copy.
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (ok) *ok = false;
        return 0;
    }
    std::uint64_t h = 0xcbf29a5e6e3560f9ULL;  // distinct seed vs. pure fnv1a64
    char buf[1 << 16];
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof buf, f);
        if (n == 0) break;
        for (std::size_t i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 0x100000001b3ULL;
        }
    }
    const bool readOk = !std::ferror(f);
    std::fclose(f);
    if (ok) *ok = readOk;
    return readOk ? h : 0;
}

}  // namespace algo1
