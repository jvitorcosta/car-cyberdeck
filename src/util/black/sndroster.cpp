#include "sndroster.h"
#include <cstring>

namespace sndroster {
namespace {
uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
}

int count(const uint8_t* blob, size_t len) {
    if (!blob || len < HEAD || memcmp(blob, "SNDR", 4) != 0 || u16(blob + 4) != VERSION) return 0;
    const int n = u16(blob + 6);
    if (n > MAX || HEAD + (size_t)n * ENTRY > len) return 0;
    return n;
}

bool at(const uint8_t* blob, size_t len, int i, Clip* out) {
    if (i < 0 || i >= count(blob, len)) return false;
    const uint8_t* e = blob + HEAD + (size_t)i * ENTRY;
    const uint32_t off = u32(e), size = u32(e + 4);
    if (off > len || size > len - off) return false;   // no off + size: it would wrap
    const char* name = (const char*)(e + 8);
    if (!memchr(name, 0, NAME)) return false;
    *out = {blob + off, size, name};
    return true;
}

}
