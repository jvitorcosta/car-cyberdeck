#include "stateparse.h"
#include <cstring>
#include "../common/jsonflat.h"

namespace stateparse {

static bool complete(const char* b) {
    if (!b) return false;
    while (*b == ' ' || *b == '\r' || *b == '\n' || *b == '\t') ++b;
    if (*b != '{') return false;
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\r' || b[n - 1] == '\n' || b[n - 1] == '\t')) --n;
    return n > 1 && b[n - 1] == '}';
}

static uint32_t u32(const char* b, const char* k) {
    long v = 0;
    return jsonflat::num(b, k, &v) && v > 0 ? (uint32_t)v : 0;
}

bool parse(const char* body, Deck& out) {
    if (!complete(body)) return false;
    Deck d{};
    d.trackGen = u32(body, "trackGen");
    d.progressMs = u32(body, "progressMs");
    d.durationMs = u32(body, "durationMs");
    d.tripS = u32(body, "tripS");
    d.lineStartMs = u32(body, "lineStartMs");
    d.nextStartMs = u32(body, "nextStartMs");
    jsonflat::boolean(body, "playing", &d.playing);
    d.hasVroom = jsonflat::arrayHas(body, "clips", "vroom");
    jsonflat::str(body, "track", d.track, sizeof d.track);
    jsonflat::str(body, "artist", d.artist, sizeof d.artist);
    jsonflat::str(body, "status", d.status, sizeof d.status);
    jsonflat::str(body, "detail", d.detail, sizeof d.detail);
    if (!jsonflat::str(body, "lyricKind", d.lyricKind, sizeof d.lyricKind))
        strcpy(d.lyricKind, "none");
    jsonflat::str(body, "line", d.line, sizeof d.line);
    jsonflat::str(body, "next", d.next, sizeof d.next);
    out = d;
    return true;
}

}
