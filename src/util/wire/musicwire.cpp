#include "musicwire.h"
#include <cstdio>
#include <cstdlib>
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
// False when the HMAC couldn't be computed: out is then unusable.
bool tag(const char* key, const uint8_t* body, uint8_t out[TAG_LEN]) {
    uint8_t full[hmac256::LEN];
    if (!hmac256::mac(reinterpret_cast<const uint8_t*>(key), strlen(key), body, BODY, full)) return false;
    memcpy(out, full, TAG_LEN);
    return true;
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
    return tag(key, out, out + BODY) ? PAL_LEN : 0;
}

bool decode(const uint8_t* in, size_t n, const char* key, PalPkt* p, Seq* s) {
    if (!in || n != PAL_LEN || in[0] != 'M' || in[1] != 'P' || in[2] != VERSION || !key || !key[0]) return false;
    uint8_t want[TAG_LEN];
    if (!tag(key, in, want) || !hmac256::same(want, in + BODY, TAG_LEN)) return false;
    s->boot = get32(in + 3);
    s->counter = get32(in + 7);
    p->songId = get32(in + 11);
    for (int i = 0; i < 3; ++i) p->p.c[i] = {in[15 + 3 * i], in[16 + 3 * i], in[17 + 3 * i]};
    p->p.cap = in[24];
    p->p.depth = in[25];
    return true;
}

bool bootProof(const char* key, uint32_t boot, const char* nonce, char out[PROOF_HEX + 1]) {
    out[0] = 0;
    if (!key || !key[0] || !nonce || strlen(nonce) != 16) return false;
    char msg[48];
    const int len = snprintf(msg, sizeof msg, "music-boot|%lu|%s", (unsigned long)boot, nonce);
    uint8_t full[hmac256::LEN];
    if (len <= 0 || (size_t)len >= sizeof msg ||
        !hmac256::mac(reinterpret_cast<const uint8_t*>(key), strlen(key), reinterpret_cast<const uint8_t*>(msg),
                      (size_t)len, full))
        return false;
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < PROOF_HEX / 2; ++i) {
        out[2 * i] = HEX[full[i] >> 4];
        out[2 * i + 1] = HEX[full[i] & 15];
    }
    out[PROOF_HEX] = 0;
    return true;
}

bool readBoot(const char* body, const char* key, const char* nonce, uint32_t* boot) {
    if (!body) return false;
    const char* b = strstr(body, "\"boot\":");
    const char* p = strstr(body, "\"proof\":\"");
    if (!b || !p) return false;
    b += 7;
    if (*b < '0' || *b > '9') return false;
    char* end = nullptr;
    const unsigned long long v = strtoull(b, &end, 10);
    if (end == b || v > 0xFFFFFFFFull) return false;
    p += 9;
    if (strlen(p) < PROOF_HEX + 1 || p[PROOF_HEX] != '"') return false;
    char want[PROOF_HEX + 1];
    if (!bootProof(key, (uint32_t)v, nonce, want)) return false;
    if (!hmac256::same(reinterpret_cast<const uint8_t*>(want), reinterpret_cast<const uint8_t*>(p), PROOF_HEX))
        return false;
    *boot = (uint32_t)v;
    return true;
}
}
