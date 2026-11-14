#include "iso9660.h"
#include <algorithm>
#include <cctype>

#include "inflate.h"

namespace vc2 {

static constexpr u32 kSector = 2048;

std::string toUpper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

IsoImage::~IsoImage() { close(); }

void IsoImage::close() {
    if (fp_) fclose(fp_);
    fp_ = nullptr;
    entries_.clear();
    cso_ = false;
    csoIndex_.clear();
    cachedBlock_ = ~0u;
}

bool IsoImage::readRaw(u64 offset, void* dst, size_t len) const {
    if (!fp_) return false;
    if (_fseeki64(fp_, (s64)offset, SEEK_SET) != 0) return false;
    return fread(dst, 1, len, fp_) == len;
}

bool IsoImage::readCsoBlock(u32 block) const {
    if (block == cachedBlock_) return true;
    if (block + 1 >= csoIndex_.size()) return false;
    const u64 start = (u64)(csoIndex_[block] & 0x7FFFFFFF) << csoAlign_;
    const u64 end = (u64)(csoIndex_[block + 1] & 0x7FFFFFFF) << csoAlign_;
    if (end < start) return false;
    cache_.resize(csoBlock_);
    const size_t size = (size_t)(end - start);
    if ((csoIndex_[block] & 0x80000000u) || size >= csoBlock_) {  // stored as is
        if (!readRaw(start, cache_.data(), csoBlock_)) return false;
    } else {
        packed_.resize(size);
        if (!readRaw(start, packed_.data(), size) || !inflateRaw(packed_.data(), size, cache_.data(), csoBlock_))
            return false;
    }
    cachedBlock_ = block;
    return true;
}

bool IsoImage::readBytes(u64 offset, void* dst, size_t len) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!fp_) return false;
    if (!cso_) return readRaw(offset, dst, len);
    if (offset + len > totalSize_) return false;
    u8* out = (u8*)dst;
    while (len) {
        const u32 block = (u32)(offset / csoBlock_), at = (u32)(offset % csoBlock_);
        if (!readCsoBlock(block)) return false;
        const size_t n = std::min<size_t>(len, csoBlock_ - at);
        memcpy(out, cache_.data() + at, n);
        out += n, offset += n, len -= n;
    }
    return true;
}

bool IsoImage::open(const std::filesystem::path& file, std::string* err) {
    close();
    fp_ = _wfopen(file.c_str(), L"rb");
    if (!fp_) {
        if (err) *err = "Could not open file";
        return false;
    }
    _fseeki64(fp_, 0, SEEK_END);
    totalSize_ = (u64)_ftelli64(fp_);
    // CSO (CISO v1): "CISO", header size, uncompressed size (u64), block size, version, index shift; then
    // one u32 per block (+1) giving where its data starts. Blocks are raw DEFLATE unless bit 31 is set.
    u8 hdr[24];
    if (readRaw(0, hdr, sizeof hdr) && memcmp(hdr, "CISO", 4) == 0) {
        const u64 size = (u64)rd32(hdr + 8) | ((u64)rd32(hdr + 12) << 32);
        csoBlock_ = rd32(hdr + 16);
        csoAlign_ = hdr[21];
        if (hdr[20] > 1 || csoBlock_ < kSector || csoBlock_ > (1u << 20) || size == 0) {
            if (err) *err = "Unsupported CSO image (only CISO version 1 is supported)";
            close();
            return false;
        }
        const u64 blocks = (size + csoBlock_ - 1) / csoBlock_;
        csoIndex_.resize((size_t)blocks + 1);
        Bytes raw(csoIndex_.size() * 4);
        if (!readRaw(24, raw.data(), raw.size())) {
            if (err) *err = "Damaged CSO image (its block index is incomplete)";
            close();
            return false;
        }
        for (size_t i = 0; i < csoIndex_.size(); i++) csoIndex_[i] = rd32(&raw[i * 4]);
        cso_ = true;
        totalSize_ = size;
    }
    u8 pvd[kSector];
    if (!readBytes(16ull * kSector, pvd, kSector) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        if (err) *err = "Not an ISO9660 image (missing primary volume descriptor)";
        close();
        return false;
    }
    volumeId_.assign((const char*)pvd + 40, 32);
    while (!volumeId_.empty() && volumeId_.back() == ' ') volumeId_.pop_back();
    const u8* root = pvd + 156;
    walk(rd32(root + 2), rd32(root + 10), "", 0);
    return true;
}

void IsoImage::walk(u32 lba, u32 size, const std::string& prefix, int depth) {
    if (depth > 16) return;
    Bytes data(((size + kSector - 1) / kSector) * kSector);
    if (!readBytes((u64)lba * kSector, data.data(), data.size())) return;
    size_t i = 0;
    while (i < data.size()) {
        u8 len = data[i];
        if (len == 0) {
            i = (i / kSector + 1) * kSector;
            continue;
        }
        const u8* r = &data[i];
        i += len;
        u8 nameLen = r[32];
        if (nameLen == 1 && (r[33] == 0 || r[33] == 1)) continue;
        std::string name((const char*)r + 33, nameLen);
        auto semi = name.find(';');
        if (semi != std::string::npos) name.resize(semi);
        IsoEntry e;
        e.path = toUpper(prefix.empty() ? name : prefix + "/" + name);
        e.lba = rd32(r + 2);
        e.size = rd32(r + 10);
        e.isDir = (r[25] & 2) != 0;
        entries_[e.path] = e;
        if (e.isDir) walk(e.lba, e.size, e.path, depth + 1);
    }
}

const IsoEntry* IsoImage::find(const std::string& path) const {
    std::string p = toUpper(path);
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    auto it = entries_.find(p);
    return it == entries_.end() ? nullptr : &it->second;
}

bool IsoImage::readFile(const std::string& path, Bytes& out) const {
    const IsoEntry* e = find(path);
    if (!e || e->isDir) return false;
    out.resize(e->size);
    return e->size == 0 || readBytes((u64)e->lba * kSector, out.data(), e->size);
}

}  // namespace vc2
