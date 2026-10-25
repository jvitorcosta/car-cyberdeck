#include "musicwire.h"

namespace musicwire {
namespace {
constexpr uint8_t BEAT = 0x01, PRESENT = 0x02;   // FeatPkt flags

bool header(const uint8_t* in, size_t n, char kind, size_t len) {
    return in && n == len && in[0] == 'M' && in[1] == (uint8_t)kind && in[2] == VERSION;
}
}  // namespace

size_t encode(const FeatPkt& f, uint8_t* out, size_t n) {
    if (n < FEAT_LEN) return 0;
    out[0] = 'M'; out[1] = 'F'; out[2] = VERSION;
    out[3] = (uint8_t)(f.seq & 0xFF);
    out[4] = (uint8_t)(f.seq >> 8);
    out[5] = (uint8_t)((f.f.beat ? BEAT : 0) | (f.f.present ? PRESENT : 0));
    out[6] = f.f.strength;
    out[7] = f.f.energy;
    out[8] = f.f.brightness;
    return FEAT_LEN;
}

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

bool decode(const uint8_t* in, size_t n, FeatPkt* f) {
    if (!header(in, n, 'F', FEAT_LEN)) return false;
    f->seq = (uint16_t)(in[3] | (in[4] << 8));
    f->f.beat = (in[5] & BEAT) != 0;
    f->f.present = (in[5] & PRESENT) != 0;
    f->f.strength = in[6];
    f->f.energy = in[7];
    f->f.brightness = in[8];
    return true;
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
