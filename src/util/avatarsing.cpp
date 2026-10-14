#include "avatarsing.h"

namespace avatarsing {
bool singing(uint32_t posMs, uint32_t lineStartMs, uint32_t nextStartMs) {
    if (posMs < lineStartMs) return false;
    const uint32_t len = nextStartMs > lineStartMs ? (nextStartMs - lineStartMs) * 3 / 4 : LONE_LINE_MS;
    return posMs - lineStartMs < len;
}

int frame(uint32_t posMs, uint32_t lineStartMs, uint32_t nextStartMs, int frames) {
    if (frames < 2 || !singing(posMs, lineStartMs, nextStartMs)) return 0;
    return ((posMs - lineStartMs) / FLAP_MS) % 2 == 0 ? 1 : 0;
}
}
