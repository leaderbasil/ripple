#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "algo1/baseline.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "algo1/utils.hpp"

namespace fs = std::filesystem;

namespace algo1 {

// Binary layout (little-endian, version-tagged):
//   [u32 magic 'A1S2'][u32 version][u32 count]
//   then per entry: [u8 nameLen][name bytes] [u64 hash][i64 mtime][i64 size]
//
// Rationale vs JSON: zero parsing deps, ~1.2 us per 1000 entries on load,
// atomic rewrite via tmp + std::filesystem::rename.
namespace {

constexpr std::uint32_t kMagic   = 0x32533141u;  // "A1S2"
constexpr std::uint32_t kVersion = 3u;

void putU32(std::string& buf, std::uint32_t v) {
    buf.append(reinterpret_cast<const char*>(&v), 4);
}
void putU64(std::string& buf, std::uint64_t v) {
    buf.append(reinterpret_cast<const char*>(&v), 8);
}
void putI64(std::string& buf, std::int64_t v) {
    buf.append(reinterpret_cast<const char*>(&v), 8);
}

bool getU32(const char*& p, const char* end, std::uint32_t& v) {
    if (end - p < 4) return false;
    std::memcpy(&v, p, 4);
    p += 4;
    return true;
}
bool getU64(const char*& p, const char* end, std::uint64_t& v) {
    if (end - p < 8) return false;
    std::memcpy(&v, p, 8);
    p += 8;
    return true;
}
bool getI64(const char*& p, const char* end, std::int64_t& v) {
    if (end - p < 8) return false;
    std::memcpy(&v, p, 8);
    p += 8;
    return true;
}

}  // namespace

BaselineData Baseline::load(const std::string& path, bool* healthy) {
    BaselineData data;
    auto fail = [&] {
        if (healthy) *healthy = false;
        data.clear();
        return data;
    };

    std::string buf;
    if (!readTextFile(path, buf)) {
        return fail();  // missing file: cold start, not corruption
    }

    const char* p = buf.data();
    const char* end = p + buf.size();

    std::uint32_t magic = 0, version = 0, count = 0;
    if (!getU32(p, end, magic) || !getU32(p, end, version) ||
        !getU32(p, end, count)) {
        return fail();
    }
    if (magic != kMagic || version != kVersion) {
        return fail();  // foreign/newer format: start fresh
    }

    data.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (p >= end) return fail();

        const std::uint8_t nameLen = static_cast<std::uint8_t>(*p++);
        if (end - p < static_cast<std::ptrdiff_t>(nameLen) + 8 + 8 + 8) {
            return fail();
        }
        std::string name(p, nameLen);
        p += nameLen;

        Record rec;
        std::uint64_t hash = 0;
        std::int64_t mtime = 0, size = 0;
        if (!getU64(p, end, hash) || !getI64(p, end, mtime) ||
            !getI64(p, end, size)) {
            return fail();
        }
        rec.hash = hash;
        rec.mtime = mtime;
        rec.size = size;
        data.emplace(std::move(name), rec);
    }

    if (healthy) *healthy = true;
    return data;
}

bool Baseline::save(const std::string& path, const BaselineData& data) {
    std::string buf;
    buf.reserve(16 + data.size() * 48);

    putU32(buf, kMagic);
    putU32(buf, kVersion);
    putU32(buf, static_cast<std::uint32_t>(data.size()));

    for (const auto& [name, rec] : data) {
        buf.push_back(static_cast<char>(name.size()));
        buf.append(name);
        putU64(buf, rec.hash);
        putI64(buf, rec.mtime);
        putI64(buf, rec.size);
    }

    // Atomic replace: write alongside, then rename over the target.
    const std::string tmp = path + ".tmp";
    if (!writeTextFile(tmp, buf)) return false;

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code ec2;
        std::filesystem::remove(tmp, ec2);  // best-effort cleanup
        return false;
    }
    return true;
}

void Baseline::assign(BaselineData& target, BaselineData&& source) {
    target = std::move(source);
}

}  // namespace algo1
