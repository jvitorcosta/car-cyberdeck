#pragma once
// What a change on the BOTH page (a tap, Y, - / +, voice or the watch without a zone) does to the
// two lights. Normally the BOTH page's look is copied onto both. But after a favourite or the
// INSIDE / OUTSIDE pages the lights can differ ("mixed": pink inside at 50 %, outside at 100 %);
// copying the page then would replace both with a look neither had. So while mixed, a change that
// carries no colour acts on each light's own look: power sets each one's on/off, a level step
// steps each one's %, an absolute level sets each one's %. A colour makes them one look again, the
// usual copy. PURE, host-tested.
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "../lights/ledlook.h"

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

// A colour on both lights that changes only the colour (the PS3 controller's right stick): each
// light keeps its own level and on/off, so a split preset stays split and a light that was off
// stays off. Not the usual copy, which would make them one look.
inline void recolour(int swatch, ledlook::Look* both, ledlook::Look* in, ledlook::Look* out) {
    both->swatch = in->swatch = out->swatch = swatch;
}

// A colour on both lights that also turns them on (the IR remote's colour keys): recolour, and
// every look lit at its own level, so a light that was off comes back at the % it had.
inline void paint(int swatch, ledlook::Look* both, ledlook::Look* in, ledlook::Look* out) {
    recolour(swatch, both, in, out);
    both->lit = in->lit = out->lit = true;
}

// The BOTH page's level line: one "60%" (or "OFF") while both lights show the same level whatever
// their colours, else each one's ("IN 60%  OUT 40%", "IN OFF  OUT 80%"; one space where two would
// pass MAX_LEVEL_TEXT, so it fits the level rect: "IN 100% OUT 90%"). Cut to fit `n`.
constexpr size_t MAX_LEVEL_TEXT = 15;
inline void levelText(const ledlook::Look& in, const ledlook::Look& out, char* buf, size_t n) {
    if (!n) return;
    auto one = [](const ledlook::Look& l, char* b, size_t m) {
        if (l.lit) snprintf(b, m, "%u%%", static_cast<unsigned>(l.pct));
        else snprintf(b, m, "OFF");
    };
    char a[8], b[8];
    one(in, a, sizeof(a));
    one(out, b, sizeof(b));
    if (strcmp(a, b) == 0) snprintf(buf, n, "%s", a);
    else snprintf(buf, n, strlen(a) + strlen(b) + 9 > MAX_LEVEL_TEXT ? "IN %s OUT %s" : "IN %s  OUT %s", a, b);
}
}  // namespace bothlook
