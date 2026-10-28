#include "fuelbook.h"
#include <cstring>

namespace fuelbook {

static bool priceInRange(float p) { return p >= PRICE_MIN && p <= PRICE_MAX; }

void fillUp(Book* b, float pricePerL) {
    if (!b) return;
    b->hasFull = true;
    b->litresSinceFull = 0;
    if (priceInRange(pricePerL)) b->pricePerL = pricePerL;  // NAN fails both comparisons
}

void burn(Book* b, float litres) {
    if (!b || !(litres > 0)) return;
    b->litresSinceFull += litres;
}

void learn(Book* b, float km, float litres) {
    if (!b || !std::isfinite(km) || !std::isfinite(litres) || !(km > 0) || !(litres > 0)) return;
    const float kmL = km / litres;
    if (std::isnan(b->recentKmL)) {
        b->recentKmL = kmL;
        return;
    }
    const float w = km / LEARN_KM < 1.0f ? km / LEARN_KM : 1.0f;
    b->recentKmL += w * (kmL - b->recentKmL);
}

float rangeKm(const Book& b) {
    if (!b.hasFull || !(b.recentKmL > 0)) return NAN;
    float km = (TANK_L - b.litresSinceFull) * b.recentKmL;
    return km > 0 ? km : 0;
}

float costRs(const Book& b, float litres) {
    if (std::isnan(b.pricePerL) || std::isnan(litres)) return NAN;
    return litres * b.pricePerL;
}

static void putF(uint8_t* out, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    for (int i = 0; i < 4; i++) out[i] = (uint8_t)(u >> (8 * i));
}

static float getF(const uint8_t* in) {
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) u |= (uint32_t)in[i] << (8 * i);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

void encode(const Book& b, uint8_t out[RECORD_BYTES]) {
    out[0] = b.hasFull ? 1 : 0;
    putF(out + 1, b.litresSinceFull);
    putF(out + 5, b.pricePerL);
    putF(out + 9, b.recentKmL);
    putF(out + 13, 0.0f);  // reserved
}

bool decode(const uint8_t* in, size_t len, Book* b) {
    if (!in || !b || len != RECORD_BYTES || in[0] > 1) return false;
    float litres = getF(in + 1);
    if (!std::isfinite(litres) || litres < 0) return false;
    b->hasFull = in[0] == 1;
    b->litresSinceFull = litres;
    // A bad price or economy is dropped to "unknown" rather than refusing the record: the fill-up and
    // the litres since are still worth keeping.
    const float price = getF(in + 5), kmL = getF(in + 9);
    b->pricePerL = std::isfinite(price) && priceInRange(price) ? price : NAN;
    b->recentKmL = std::isfinite(kmL) && kmL > 0 ? kmL : NAN;
    return true;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static bool isLetter(char c) { c = lower(c); return c >= 'a' && c <= 'z'; }
static bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool parsePhrase(const char* text, float* price, bool* priceOk) {
    float p = NAN;
    bool ok = true;
    if (price) *price = p;
    if (priceOk) *priceOk = ok;
    if (!text) return false;

    // words: runs of letters, lowercased (longer than any word we look for are just skipped)
    bool fill = false, full = false, tank = false, tanque = false, cheio = false;
    for (const char* s = text; *s;) {
        if (!isLetter(*s)) { s++; continue; }
        char w[16];
        size_t n = 0;
        while (isLetter(*s)) { if (n < sizeof w - 1) w[n] = lower(*s); n++; s++; }
        if (n >= sizeof w) continue;
        w[n] = 0;
        if (!strcmp(w, "filled") || !strcmp(w, "fill") || !strcmp(w, "abasteci") || !strcmp(w, "abastecido")) fill = true;
        else if (!strcmp(w, "full")) full = true;
        else if (!strcmp(w, "tank")) tank = true;
        else if (!strcmp(w, "tanque")) tanque = true;
        else if (!strcmp(w, "cheio")) cheio = true;
    }
    if (!(fill || (full && tank) || (tanque && cheio))) return false;

    // the first number: digits, then '.' or ',' and one or two more digits (a bare "6." is just 6)
    for (const char* s = text; *s; s++) {
        if (!isDigit(*s)) continue;
        float v = 0;
        while (isDigit(*s)) { v = v * 10.0f + (float)(*s - '0'); s++; }
        if ((*s == '.' || *s == ',') && isDigit(s[1])) {
            float scale = 0.1f;
            s++;
            for (int i = 0; i < 2 && isDigit(*s); i++, s++) { v += (float)(*s - '0') * scale; scale *= 0.1f; }
        }
        p = v;
        ok = priceInRange(v);
        break;
    }
    if (price) *price = p;
    if (priceOk) *priceOk = ok;
    return true;
}

}  // namespace fuelbook
