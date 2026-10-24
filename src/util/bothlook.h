#pragma once
// What a change on the BOTH page (a tap, Y, - / +, voice or the watch without a zone) does to the
// two lights. Normally the BOTH page's look is copied onto both. But after a favourite or the
// INSIDE / OUTSIDE pages the lights can differ ("mixed": pink inside at 50 %, outside at 100 %);
// copying the page then would replace both with a look neither had. So while mixed, a change that
// carries no colour acts on each light's own look: power sets each one's on/off, a level step
// steps each one's %, an absolute level sets each one's %. A colour makes them one look again, the
// usual copy. PURE, host-tested.
#include "ledlook.h"

namespace bothlook {
// One change to a look; -1 / 0 leave that part as it is.
struct Change {
    int swatch = -1;  // a colour (also turns it on), or -1
    int power = -1;   // 1 on, 0 off, -1 unchanged; applied after the colour ("pink off" stays off)
    int level = -1;   // absolute %, or -1
    int step = 0;     // +1 / -1: one 10 % step
};

inline bool mixed(const ledlook::Look& in, const ledlook::Look& out) { return !(in == out); }

// The BOTH page's look while the lights differ: the inside one, lit if either is, so the page never
// stands for a third colour (and a voice reply describing it names a colour a light really shows).
inline ledlook::Look seed(const ledlook::Look& in, const ledlook::Look& out) {
    ledlook::Look b = in;
    b.lit = in.lit || out.lit;
    return b;
}

// The change on one look: what a tap on a page always did. The % is clamped to MIN_PCT..100.
inline void edit(ledlook::Look* l, const Change& c) {
    if (c.swatch >= 0) {
        l->swatch = c.swatch;
        l->lit = true;
    }
    if (c.power >= 0) l->lit = c.power == 1;
    const int pct = c.level >= 0 ? c.level : l->pct + 10 * c.step;
    l->pct = static_cast<uint8_t>(pct < ledlook::MIN_PCT ? ledlook::MIN_PCT : pct > 100 ? 100 : pct);
}

// The change on the BOTH page: `both` is the page's look, `in` / `out` the lights'.
inline void apply(const Change& c, ledlook::Look* both, ledlook::Look* in, ledlook::Look* out) {
    if (!mixed(*in, *out) || c.swatch >= 0) {  // the page's look onto both lights, as always
        edit(both, c);
        *in = *out = *both;
        return;
    }
    edit(in, c);
    edit(out, c);
    *both = seed(*in, *out);  // equal again (a step that met at 100 %): the page is that look
}
}  // namespace bothlook
