#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "ripple/utils.hpp"

#include <cstdio>
#include <filesystem>

namespace ripple {

bool readTextFile(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[1 << 16];
    out.clear();
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof buf, f);
        if (n == 0) break;
        out.append(buf, n);
    }
    const bool ok = !std::ferror(f);
    std::fclose(f);

    // Tolerate a UTF-8 BOM: it would otherwise corrupt the first line's
    // `#`-at-line-start check and silently drop one dependency edge.
    if (out.size() >= 3 &&
        static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB &&
        static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return ok;
}

bool writeTextFile(const std::string& path, const std::string& content) {
    const std::filesystem::path p{path};
    const auto parent = p.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);  // best effort
    }
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(content.data(), 1, content.size(), f) == content.size();
    const bool closed = std::fclose(f) == 0;
    return ok && closed;
}

}  // namespace ripple
