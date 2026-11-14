#pragma once
// Minimal read-only ISO9660 reader for PSP UMD images: plain .iso, or compressed .cso (CISO v1).
#include "types.h"
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace vc2 {

struct IsoEntry {
    std::string path;  // upper-case, '/'-separated, no leading slash
    u32 lba = 0;
    u32 size = 0;
    bool isDir = false;
};

class IsoImage {
public:
    ~IsoImage();
    bool open(const std::filesystem::path& file, std::string* err = nullptr);
    void close();
    bool isOpen() const { return fp_ != nullptr; }

    // Returns nullptr if not found. Lookup is case-insensitive.
    const IsoEntry* find(const std::string& path) const;
    const std::map<std::string, IsoEntry>& entries() const { return entries_; }

    bool readFile(const std::string& path, Bytes& out) const;
    // Sector-level access to the (uncompressed) image (thread-safe).
    bool readBytes(u64 offset, void* dst, size_t len) const;
    u64 totalSize() const { return totalSize_; }  // uncompressed size
    bool compressed() const { return cso_; }
    std::string volumeId() const { return volumeId_; }

private:
    void walk(u32 lba, u32 size, const std::string& prefix, int depth);
    bool readRaw(u64 offset, void* dst, size_t len) const;  // the file itself (lock held)
    bool readCsoBlock(u32 block) const;                     // into cache_ (lock held)
    FILE* fp_ = nullptr;
    // CSO: index of block offsets (bit 31: stored uncompressed), shifted left by csoAlign_
    bool cso_ = false;
    u32 csoBlock_ = 2048, csoAlign_ = 0;
    std::vector<u32> csoIndex_;
    mutable std::vector<u8> cache_, packed_;
    mutable u32 cachedBlock_ = ~0u;
    mutable std::mutex mtx_;
    std::map<std::string, IsoEntry> entries_;
    u64 totalSize_ = 0;
    std::string volumeId_;
};

std::string toUpper(std::string s);

}  // namespace vc2
