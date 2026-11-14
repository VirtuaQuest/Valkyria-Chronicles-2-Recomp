#include "inflate.h"

#include <cstring>

namespace vc2 {

namespace {

struct Bits {
    const u8* p;
    size_t len, pos = 0;  // pos in bits
    bool overrun = false;

    u32 get(int n) {
        u32 v = 0;
        for (int i = 0; i < n; i++, pos++) {
            const size_t byte = pos >> 3;
            if (byte >= len) {
                overrun = true;
                return 0;
            }
            v |= ((p[byte] >> (pos & 7)) & 1u) << i;
        }
        return v;
    }
    void alignByte() { pos = (pos + 7) & ~size_t(7); }
};

// Canonical Huffman decoding table (counts per code length, symbols ordered by code).
struct Huffman {
    u16 count[16] = {};
    u16 symbol[320] = {};

    bool build(const u8* lengths, int n) {
        memset(count, 0, sizeof count);
        for (int i = 0; i < n; i++) count[lengths[i]]++;
        count[0] = 0;
        u16 offs[16] = {};
        for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + count[i - 1];
        for (int i = 0; i < n; i++)
            if (lengths[i]) symbol[offs[lengths[i]]++] = (u16)i;
        return true;
    }
    int decode(Bits& b) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; len++) {
            code |= (int)b.get(1);
            if (b.overrun) return -1;
            const int c = count[len];
            if (code - c < first) return symbol[index + (code - first)];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

const u16 kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const u8 kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const u16 kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                           257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const u8 kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(Bits& b, const Huffman& lit, const Huffman& dist, u8* out, size_t outLen, size_t& o) {
    for (;;) {
        int sym = lit.decode(b);
        if (sym < 0) return false;
        if (sym < 256) {
            if (o >= outLen) return false;
            out[o++] = (u8)sym;
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            const size_t len = kLenBase[sym] + b.get(kLenExtra[sym]);
            const int ds = dist.decode(b);
            if (ds < 0 || ds >= 30) return false;
            const size_t d = kDistBase[ds] + b.get(kDistExtra[ds]);
            if (b.overrun || d > o || o + len > outLen) return false;
            for (size_t i = 0; i < len; i++, o++) out[o] = out[o - d];
        }
    }
}

}  // namespace

bool inflateRaw(const u8* in, size_t inLen, u8* out, size_t outLen) {
    Bits b{in, inLen};
    size_t o = 0;
    for (bool last = false; !last;) {
        last = b.get(1) != 0;
        const u32 type = b.get(2);
        if (b.overrun) return false;
        if (type == 0) {  // stored
            b.alignByte();
            const u32 n = b.get(16), nn = b.get(16);
            if (b.overrun || (n ^ 0xFFFF) != nn) return false;
            const size_t byte = b.pos >> 3;
            if (byte + n > inLen || o + n > outLen) return false;
            memcpy(out + o, in + byte, n);
            o += n;
            b.pos += (size_t)n * 8;
        } else if (type == 1) {  // fixed Huffman codes
            static Huffman lit, dist;
            static const bool ready = [] {
                u8 l[288];
                int i = 0;
                for (; i < 144; i++) l[i] = 8;
                for (; i < 256; i++) l[i] = 9;
                for (; i < 280; i++) l[i] = 7;
                for (; i < 288; i++) l[i] = 8;
                lit.build(l, 288);
                u8 d[30];
                for (auto& x : d) x = 5;
                dist.build(d, 30);
                return true;
            }();
            (void)ready;
            if (!codes(b, lit, dist, out, outLen, o)) return false;
        } else if (type == 2) {  // dynamic Huffman codes
            const int nlen = (int)b.get(5) + 257, ndist = (int)b.get(5) + 1, ncode = (int)b.get(4) + 4;
            if (b.overrun || nlen > 286 || ndist > 30) return false;
            static const u8 order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            u8 lengths[320] = {};
            for (int i = 0; i < ncode; i++) lengths[order[i]] = (u8)b.get(3);
            Huffman lencode;
            lencode.build(lengths, 19);
            int index = 0;
            while (index < nlen + ndist) {
                const int sym = lencode.decode(b);
                if (sym < 0) return false;
                if (sym < 16) {
                    lengths[index++] = (u8)sym;
                    continue;
                }
                u8 len = 0;
                int rep;
                if (sym == 16) {
                    if (index == 0) return false;
                    len = lengths[index - 1];
                    rep = 3 + (int)b.get(2);
                } else if (sym == 17) {
                    rep = 3 + (int)b.get(3);
                } else {
                    rep = 11 + (int)b.get(7);
                }
                if (b.overrun || index + rep > nlen + ndist) return false;
                while (rep--) lengths[index++] = len;
            }
            if (lengths[256] == 0) return false;
            Huffman lit, dist;
            lit.build(lengths, nlen);
            dist.build(lengths + nlen, ndist);
            if (!codes(b, lit, dist, out, outLen, o)) return false;
        } else {
            return false;
        }
    }
    return o == outLen;
}

}  // namespace vc2
