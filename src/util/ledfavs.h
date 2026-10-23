#pragma once
// The panel's favourites: four slots, each a pair of looks (inside light, outside light). Stored as
// 6 bytes per slot so they survive in NVS; a slot with any invalid byte reads back as empty rather
// than driving the lights with garbage. PURE, host-tested.
#include <cstddef>
#include <cstdint>

#include "ledlook.h"

namespace ledfavs {
constexpr int N_FAV = 4;
constexpr int BYTES = 6;   // inSwatch, inPct, inLit, outSwatch, outPct, outLit

struct Fav {
    bool set = false;
    ledlook::Look in, out;
};

void encode(const Fav& f, uint8_t out[BYTES]);
// false and *f = {} (empty) unless every byte is valid: swatch < 8, pct 10..100 in steps of 10, lit 0/1
bool decode(const uint8_t in[BYTES], Fav* f);
// slot is 1..4. "Favourite 1: inside Pink 50 percent, outside Pink 100 percent"; a light that's
// off reads "inside off"; empty: "Favourite 3 is empty"
void describe(const Fav& f, int slot, char* out, size_t n);
// one /favs line: "1 pink 50 pink 100"; off light "off 0"; empty "3 -"
void wireLine(const Fav& f, int slot, char* out, size_t n);
}  // namespace ledfavs
