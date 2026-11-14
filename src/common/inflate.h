#pragma once
// Raw DEFLATE (RFC 1951) decoding, for the blocks of CSO disc images.
#include <cstddef>

#include "types.h"

namespace vc2 {

// Decodes a raw DEFLATE stream (no zlib or gzip header) into exactly `outLen` bytes. False on malformed
// or truncated input, or when the stream holds more or fewer bytes than `outLen`.
bool inflateRaw(const u8* in, size_t inLen, u8* out, size_t outLen);

}  // namespace vc2
