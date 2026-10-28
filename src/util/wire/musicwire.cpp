#include "musicwire.h"
#include <cstring>
#include "hmac256.h"

namespace musicwire {
namespace {
constexpr size_t BODY = PAL_LEN - TAG_LEN;   // what the tag covers

void put32(uint8_t* at, uint32_t v) {
    for (int i = 0; i < 4; ++i) at[i] = (uint8_t)(v >> (8 * i));
}
uint32_t get32(const uint8_t* at) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (uint32_t)at[i] << (8 * i);
    return v;
}
void tag(const char* key, const uint8_t* body, uint8_t out[TAG_LEN]) {
    uint8_t full[hmac256::LEN];
    hmac256::mac(reinterpret_cast<const uint8_t*>(key), strlen(key), body, BODY, full);
    memcpy(out, full, TAG_LEN);
}
}  // namespace

size_t encode(const PalPkt& p, Seq s, const char* key, uint8_t* out, size_t n) {
    if (n < PAL_LEN || !key || !key[0]) return 0;
    out[0] = 'M'; out[1] = 'P'; out[2] = VERSION;
    put32(out + 3, s.boot);
    put32(out + 7, s.counter);
    put32(out + 11, p.songId);
    for (int i = 0; i < 3; ++i) {
        out[15 + 3 * i] = p.p.c[i].r;
        out[16 + 3 * i] = p.p.c[i].g;
        out[17 + 3 * i] = p.p.c[i].b;
    }
    out[24] = p.p.cap;
    out[25] = p.p.depth;
    tag(key, out, out + BODY);
    return PAL_LEN;
}

bool decode(const uint8_t* in, size_t n, const char* key, PalPkt* p, Seq* s) {
    if (!in || n != PAL_LEN || in[0] != 'M' || in[1] != 'P' || in[2] != VERSION || !key || !key[0]) return false;
    uint8_t want[TAG_LEN];
    tag(key, in, want);
    if (!hmac256::same(want, in + BODY, TAG_LEN)) return false;
    s->boot = get32(in + 3);
    s->counter = get32(in + 7);
    p->songId = get32(in + 11);
    for (int i = 0; i < 3; ++i) p->p.c[i] = {in[15 + 3 * i], in[16 + 3 * i], in[17 + 3 * i]};
    p->p.cap = in[24];
    p->p.depth = in[25];
    return true;
}
}
