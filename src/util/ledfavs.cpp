#include "ledfavs.h"

#include <cstdio>

namespace ledfavs {
namespace {
bool validLook(uint8_t swatch, uint8_t pct, uint8_t lit) {
    return swatch < ledlook::N_SWATCH && pct >= ledlook::MIN_PCT && pct <= 100 && pct % 10 == 0 && lit <= 1;
}
ledlook::Look lookAt(const uint8_t* b) {
    ledlook::Look l;
    l.swatch = b[0];
    l.pct = b[1];
    l.lit = b[2] != 0;
    return l;
}
void putLook(const ledlook::Look& l, uint8_t* b) {
    b[0] = static_cast<uint8_t>(l.swatch);
    b[1] = l.pct;
    b[2] = l.lit ? 1 : 0;
}
void spokenLook(const ledlook::Look& l, const char* zone, char* out, size_t n) {
    if (!l.lit) snprintf(out, n, "%s off", zone);
    else snprintf(out, n, "%s %s %u percent", zone, ledlook::SWATCHES[l.swatch].name, static_cast<unsigned>(l.pct));
}
void wireLook(const ledlook::Look& l, char* out, size_t n) {
    if (!l.lit) snprintf(out, n, "off 0");
    else snprintf(out, n, "%s %u", ledlook::SWATCHES[l.swatch].word, static_cast<unsigned>(l.pct));
}
}  // namespace

void encode(const Fav& f, uint8_t out[BYTES]) {
    putLook(f.in, out);
    putLook(f.out, out + 3);
}

bool decode(const uint8_t in[BYTES], Fav* f) {
    if (!validLook(in[0], in[1], in[2]) || !validLook(in[3], in[4], in[5])) {
        *f = Fav{};
        return false;
    }
    f->set = true;
    f->in = lookAt(in);
    f->out = lookAt(in + 3);
    return true;
}

void describe(const Fav& f, int slot, char* out, size_t n) {
    if (!f.set) {
        snprintf(out, n, "Favourite %d is empty", slot);
        return;
    }
    char a[48], b[48];
    spokenLook(f.in, "inside", a, sizeof a);
    spokenLook(f.out, "outside", b, sizeof b);
    snprintf(out, n, "Favourite %d: %s, %s", slot, a, b);
}

void wireLine(const Fav& f, int slot, char* out, size_t n) {
    if (!f.set) {
        snprintf(out, n, "%d -", slot);
        return;
    }
    char a[24], b[24];
    wireLook(f.in, a, sizeof a);
    wireLook(f.out, b, sizeof b);
    snprintf(out, n, "%d %s %s", slot, a, b);
}
}  // namespace ledfavs
