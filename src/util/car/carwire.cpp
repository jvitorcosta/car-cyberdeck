#include "carwire.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../common/jsonflat.h"
#include "../common/jsonpath.h"

namespace carwire {
namespace {

// Appends into out[0..n) and remembers if anything didn't fit, so encode() checks once at the end.
struct Out {
    char* buf;
    size_t n, len = 0;
    bool over = false;
    void raw(const char* s, size_t k) {
        if (over || len + k >= n) { over = true; return; }  // keeps room for the NUL
        memcpy(buf + len, s, k);
        len += k;
    }
    void lit(const char* s) { raw(s, strlen(s)); }
    void key(const char* k) {
        if (len > 1) lit(",");
        lit("\"");
        lit(k);
        lit("\":");
    }
    void real(const char* k, float v, int decimals) {
        key(k);
        if (!std::isfinite(v)) { lit("null"); return; }
        char t[24];
        int w = snprintf(t, sizeof(t), "%.*f", decimals, static_cast<double>(v));
        if (w < 0 || static_cast<size_t>(w) >= sizeof(t)) { over = true; return; }
        raw(t, static_cast<size_t>(w));
    }
    void uint(const char* k, uint32_t v) {
        key(k);
        char t[16];
        int w = snprintf(t, sizeof(t), "%lu", static_cast<unsigned long>(v));
        raw(t, static_cast<size_t>(w));
    }
    void text(const char* k, const char* s) {
        key(k);
        lit("\"");
        for (const char* p = s; p && *p; p++) {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c == '"' || c == '\\') {
                const char e[2] = {'\\', *p};
                raw(e, 2);
            } else if (c < 0x20) {
                char e[8];
                int w = snprintf(e, sizeof(e), "\\u%04x", static_cast<unsigned>(c));
                raw(e, static_cast<size_t>(w));
            } else {
                raw(p, 1);
            }
        }
        lit("\"");
    }
};

const char* const LINKS[] = {"off", "connecting", "noengine", "live"};

void putVerdict(Out& o, const char* kl, const char* kk, const char* kw, const carhealth::Verdict& v) {
    o.uint(kl, static_cast<uint32_t>(v.level));
    o.uint(kk, v.known ? 1u : 0u);
    o.text(kw, v.why);
}

void putSummary(Out& o, const char* kkm, const char* kl, const char* kkml, const char* kavg, const char* ks,
                const char* kidle, const tripsum::Summary& s) {
    o.real(kkm, s.km, 2);
    o.real(kl, s.litres, 3);
    o.real(kkml, s.kmL, 1);
    o.real(kavg, s.avgKmh, 1);
    o.uint(ks, s.seconds);
    o.uint(kidle, s.idleS);
}

// The value at a flat key, or nullptr.
const char* at(const char* json, const char* key) { return jsonpath::find(json, key); }

bool isNull(const char* p) { return strncmp(p, "null", 4) == 0; }

// Optional number: absent keeps *v, null -> NAN, a number is read, anything else is junk.
bool getReal(const char* json, const char* key, float* v) {
    const char* p = at(json, key);
    if (!p) return true;
    if (isNull(p)) { *v = NAN; return true; }
    char* end = nullptr;
    const double d = strtod(p, &end);
    if (end == p) return false;
    *v = static_cast<float>(d);
    return true;
}

bool getUint(const char* json, const char* key, uint32_t* v) {
    const char* p = at(json, key);
    if (!p) return true;
    if (*p < '0' || *p > '9') return false;
    char* end = nullptr;
    const unsigned long u = strtoul(p, &end, 10);
    if (end == p || u > 0xFFFFFFFFul) return false;
    *v = static_cast<uint32_t>(u);
    return true;
}

bool getFlag(const char* json, const char* key, bool* v) {
    uint32_t u = *v ? 1u : 0u;
    if (!getUint(json, key, &u) || u > 1) return false;
    *v = u != 0;
    return true;
}

bool getVerdict(const char* json, const char* kl, const char* kk, const char* kw, carhealth::Verdict* v) {
    uint32_t level = static_cast<uint32_t>(v->level);
    if (!getUint(json, kl, &level) || level > 2) return false;
    v->level = static_cast<carhealth::Level>(level);
    if (!getFlag(json, kk, &v->known)) return false;
    const char* p = at(json, kw);
    if (p && !jsonflat::strAt(p, v->why, sizeof(v->why))) return false;  // cut to fit, never overruns
    return true;
}

bool getSummary(const char* json, const char* kkm, const char* kl, const char* kkml, const char* kavg,
                const char* ks, const char* kidle, tripsum::Summary* s) {
    return getReal(json, kkm, &s->km) && getReal(json, kl, &s->litres) && getReal(json, kkml, &s->kmL) &&
           getReal(json, kavg, &s->avgKmh) && getUint(json, ks, &s->seconds) && getUint(json, kidle, &s->idleS);
}

}  // namespace

size_t encode(const Snapshot& s, char* out, size_t n) {
    if (!out || n == 0) return 0;
    Out o{out, n};
    o.lit("{");
    o.uint("v", 1);
    o.uint("seq", s.seq);
    const unsigned li = static_cast<unsigned>(s.link);
    o.key("link");
    o.lit("\"");
    o.lit(LINKS[li < 4 ? li : 0]);
    o.lit("\"");
    o.uint("ready", s.checkReady ? 1u : 0u);
    o.real("kmh", s.kmh, 1);
    o.real("rpm", s.rpm, 1);
    o.real("volts", s.volts, 2);
    o.real("cool", s.coolantC, 1);
    o.real("kml", s.kmlNow, 1);
    putVerdict(o, "bl", "bk", "bw", s.battery);
    putVerdict(o, "el", "ek", "ew", s.engine);
    putSummary(o, "tkm", "tl", "tkml", "tavg", "ts", "tidle", s.trip);
    o.real("tcost", s.tripCostRs, 2);
    o.uint("last", s.hasLast ? 1u : 0u);
    putSummary(o, "lkm", "ll", "lkml", "lavg", "ls", "lidle", s.last);
    o.real("lcost", s.lastCostRs, 2);
    o.real("range", s.rangeKm, 1);
    o.real("price", s.pricePerL, 2);
    o.uint("crit", s.critical ? 1u : 0u);
    o.lit("}");
    if (o.over) {
        out[0] = '\0';
        return 0;
    }
    out[o.len] = '\0';
    return o.len;
}

bool decode(const char* json, Snapshot* s) {
    if (!json || !s) return false;
    // A body cut off anywhere has lost its closing brace.
    const char* a = json;
    while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') a++;
    size_t len = strlen(a);
    while (len > 0 && (a[len - 1] == ' ' || a[len - 1] == '\t' || a[len - 1] == '\r' || a[len - 1] == '\n')) len--;
    if (len < 2 || a[0] != '{' || a[len - 1] != '}') return false;

    uint32_t v = 0;
    const char* pv = at(json, "v");
    if (!pv || *pv < '0' || *pv > '9' || !getUint(json, "v", &v) || v != 1) return false;

    Snapshot t;  // defaults for whatever the body leaves out
    if (!getUint(json, "seq", &t.seq)) return false;
    if (const char* pl = at(json, "link")) {
        char name[16];
        if (!jsonflat::strAt(pl, name, sizeof(name))) return false;
        size_t i = 0;
        while (i < 4 && strcmp(name, LINKS[i]) != 0) i++;
        if (i == 4) return false;
        t.link = static_cast<Link>(i);
    }
    if (!getFlag(json, "ready", &t.checkReady) || !getReal(json, "kmh", &t.kmh) || !getReal(json, "rpm", &t.rpm) ||
        !getReal(json, "volts", &t.volts) || !getReal(json, "cool", &t.coolantC) ||
        !getReal(json, "kml", &t.kmlNow) || !getVerdict(json, "bl", "bk", "bw", &t.battery) ||
        !getVerdict(json, "el", "ek", "ew", &t.engine) ||
        !getSummary(json, "tkm", "tl", "tkml", "tavg", "ts", "tidle", &t.trip) ||
        !getReal(json, "tcost", &t.tripCostRs) || !getFlag(json, "last", &t.hasLast) ||
        !getSummary(json, "lkm", "ll", "lkml", "lavg", "ls", "lidle", &t.last) ||
        !getReal(json, "lcost", &t.lastCostRs) || !getReal(json, "range", &t.rangeKm) ||
        !getReal(json, "price", &t.pricePerL) || !getFlag(json, "crit", &t.critical))
        return false;
    *s = t;
    return true;
}

}  // namespace carwire
