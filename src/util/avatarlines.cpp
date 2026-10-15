#include "avatarlines.h"
#include <cstring>
#include "avatarbin.h"

namespace avatarlines {
using avatarbin::Happy;
using avatarbin::Smug;
using avatarbin::Talk;

const Line BOOT[BOOT_N] = {
    {Happy, "Darling~ I'm here. Let's go for a drive!"},
    {Happy, "Hi darling~ Where are we going today?"},
    {Smug, "Took you long enough, darling."},
};
const Line TRACK[TRACK_N] = {
    {Smug, "Darling, {A} again?"},
    {Talk, "Ooh, {T}! Turn it up~"},
    {Happy, "I like this one, darling!"},
    {Talk, "A wild {P}! Bet it can't keep up with us."},
    {Smug, "{P} is cute. Not as cute as me."},
    {Talk, "Sing along, darling. I won't laugh. Much."},
    {Happy, "This one's for the road~"},
    {Talk, "{A}? You have good taste, darling."},
    {Smug, "Eyes on the road, darling."},
    {Talk, "{P} wants to ride with us."},
    {Happy, "Faster, darling! ...Kidding. Drive safe."},
    {Talk, "Just us, the road and {T}."},
};
const Line REACT[REACT_N] = {
    {Happy, "Awoo~! The whole street heard that."},
    {Happy, "Vroom vroom~ Faster, darling!"},
    {Talk, "New trip! Where to, darling?"},
    {Smug, "One at a time, darling!"},
    {Smug, "I can't reach the deck, darling."},
    {Happy, "Car connected, darling! Let's see what she can do~"},
    {Talk, "The car's asleep~"},
};

// Appends len bytes of s at out[o], whole UTF-8 characters only; false once one doesn't fit.
static bool append(char* out, size_t n, size_t& o, const char* s, size_t len, bool upper) {
    for (size_t i = 0; i < len;) {
        const unsigned char c = (unsigned char)s[i];
        size_t cl = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        if (i + cl > len) cl = len - i;
        if (o + cl + 1 > n) return false;
        for (size_t k = 0; k < cl; ++k) {
            const char ch = s[i + k];
            out[o++] = (upper && ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
        }
        i += cl;
    }
    return true;
}

void fill(const char* tmpl, const char* title, const char* artist, const char* poke, char* out,
          size_t n) {
    if (!out || n == 0) return;
    const char* T = title && *title ? title : "this song";
    const char* A = artist && *artist ? artist : "this artist";
    const char* P = poke && *poke ? poke : "POK\xC3\xA9MON";
    size_t o = 0;
    for (const char* p = tmpl ? tmpl : ""; *p;) {
        if (p[0] == '{' && (p[1] == 'T' || p[1] == 'A' || p[1] == 'P') && p[2] == '}') {
            const char* v = p[1] == 'T' ? T : p[1] == 'A' ? A : P;
            if (!append(out, n, o, v, strlen(v), p[1] == 'P')) break;
            p += 3;
            continue;
        }
        const char* brace = strchr(p + 1, '{');             // literal run up to the next brace
        const size_t lit = brace ? (size_t)(brace - p) : strlen(p);
        if (!append(out, n, o, p, lit, false)) break;
        p += lit;
    }
    out[o] = '\0';
}

bool pick(const In& in, Out& out) {
    if (in.event >= Event::Howl) {                         // reactions: always, fixed line
        const Line& r = REACT[(int)in.event - (int)Event::Howl];
        out.mood = r.mood;
        fill(r.text, in.title, in.artist, in.poke, out.text, sizeof out.text);
        return true;
    }
    const Line* l;
    if (in.event == Event::Boot) {
        l = &BOOT[in.rnd % BOOT_N];
    } else {
        if (in.spokeBefore && in.sinceLastMs < GAP_MS) return false;
        if (in.rnd % ODDS != 0) return false;
        const bool noPoke = !in.poke || !in.poke[0];
        const uint32_t first = (in.rnd / ODDS) % TRACK_N;
        l = &TRACK[first];
        for (uint32_t k = 0; k < (uint32_t)TRACK_N; ++k) {   // no Pokemon: skip the {P} lines
            l = &TRACK[(first + k) % TRACK_N];
            if (!noPoke || !strstr(l->text, "{P}")) break;
        }
    }
    out.mood = l->mood;
    fill(l->text, in.title, in.artist, in.poke, out.text, sizeof out.text);
    return true;
}
}
