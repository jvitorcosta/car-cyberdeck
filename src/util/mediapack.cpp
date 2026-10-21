#include "mediapack.h"
#include <cstring>

namespace mediapack {
namespace {
uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
}

uint32_t total(const uint8_t* head, size_t len) {
    if (!head || len < HEAD || memcmp(head, "CDMEDIA1", 8) != 0 || u16(head + 8) != VERSION) return 0;
    const uint32_t n = u16(head + 10), size = u32(head + 12);
    if (size < HEAD + n * ENTRY) return 0;
    return size;
}

int count(const uint8_t* pack, size_t len) {
    const uint32_t size = total(pack, len);
    return size && size <= len ? u16(pack + 10) : 0;
}

bool at(const uint8_t* pack, size_t len, int i, File* out) {
    if (i < 0 || i >= count(pack, len)) return false;
    const uint8_t* e = pack + HEAD + (size_t)i * ENTRY;
    const uint32_t off = u32(e + NAME), size = u32(e + NAME + 4), size_all = u32(pack + 12);
    if (off < HEAD || off > size_all || size > size_all - off) return false;
    *out = {pack + off, size, u32(e + NAME + 8)};
    return true;
}

int find(const uint8_t* pack, size_t len, const char* name) {
    if (!name || strlen(name) >= NAME) return -1;
    const int n = count(pack, len);
    for (int i = 0; i < n; ++i)
        if (strncmp((const char*)(pack + HEAD + (size_t)i * ENTRY), name, NAME) == 0) return i;
    return -1;
}

uint32_t crc32(const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {   // racing builders write the same values
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        built = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
}
