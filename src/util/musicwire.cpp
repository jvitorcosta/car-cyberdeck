#include "musicwire.h"

namespace musicwire {
namespace {
bool header(const uint8_t* in, size_t n, char kind, size_t len) {
    return in && n == len && in[0] == 'M' && in[1] == (uint8_t)kind && in[2] == VERSION;
}
}  // namespace

size_t encode(const PalPkt& p, uint8_t* out, size_t n) {
    if (n < PAL_LEN) return 0;
    out[0] = 'M'; out[1] = 'P'; out[2] = VERSION;
    for (int i = 0; i < 4; ++i) out[3 + i] = (uint8_t)(p.songId >> (8 * i));
    for (int i = 0; i < 3; ++i) {
        out[7 + 3 * i] = p.p.c[i].r;
        out[8 + 3 * i] = p.p.c[i].g;
        out[9 + 3 * i] = p.p.c[i].b;
    }
    out[16] = p.p.cap;
    out[17] = p.p.depth;
    return PAL_LEN;
}

bool decode(const uint8_t* in, size_t n, PalPkt* p) {
    if (!header(in, n, 'P', PAL_LEN)) return false;
    p->songId = 0;
    for (int i = 0; i < 4; ++i) p->songId |= (uint32_t)in[3 + i] << (8 * i);
    for (int i = 0; i < 3; ++i) p->p.c[i] = {in[7 + 3 * i], in[8 + 3 * i], in[9 + 3 * i]};
    p->p.cap = in[16];
    p->p.depth = in[17];
    return true;
}
}
