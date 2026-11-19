#include "tripbook.h"
#include <cmath>

namespace tripbook {

bool Meter::tick(uint32_t nowMs, bool live, const Totals& t, fuelbook::Book* book) {
    if (!started_) {           // the totals start at 0 with the power-on, like this meter
        started_ = true;
        lastMs_ = chargedAt_ = nowMs;
        return false;
    }
    uint32_t dt = nowMs - lastMs_;   // unsigned: right across the wrap
    lastMs_ = nowMs;
    if (dt > MAX_GAP_MS) dt = MAX_GAP_MS;
    if (live) liveMs_ += dt;
    if (nowMs - chargedAt_ < CHARGE_MS) return false;
    return charge(nowMs, t, book);
}

bool Meter::charge(uint32_t nowMs, const Totals& t, fuelbook::Book* book) {
    chargedAt_ = nowMs;
    if (t.airSteps == steps_ || !std::isfinite(t.litres) || !std::isfinite(t.km)) return false;
    const float litres = t.litres - litres_, km = t.km - km_;
    steps_ = t.airSteps;
    litres_ = t.litres;
    km_ = t.km;
    if (!(litres > 0.0f)) return false;
    fuelbook::burn(book, litres);
    if (km > 0.0f) learnKm_ += km;
    learnL_ += litres;
    if (learnKm_ >= LEARN_MIN_KM) {
        fuelbook::learn(book, learnKm_, learnL_);
        learnKm_ = learnL_ = 0.0f;
    }
    return true;
}

bool parsePrice(const char* s, float* price) {
    if (!s || !price) return false;
    while (*s == ' ') ++s;
    int whole = 0, digits = 0;
    for (; *s >= '0' && *s <= '9'; ++s, ++digits) {
        if (digits >= 2) return false;           // 3 digits is never a price per litre ("649")
        whole = whole * 10 + (*s - '0');
    }
    if (!digits) return false;
    int cents = 0;
    if (*s == '.' || *s == ',') {
        ++s;
        int d = 0;
        for (; *s >= '0' && *s <= '9'; ++s, ++d) {
            if (d >= 2) return false;
            cents = cents * 10 + (*s - '0');
        }
        if (!d) return false;
        if (d == 1) cents *= 10;
    }
    while (*s == ' ') ++s;
    if (*s) return false;
    const float p = whole + cents / 100.0f;
    if (p < fuelbook::PRICE_MIN || p > fuelbook::PRICE_MAX) return false;
    *price = p;
    return true;
}
}  // namespace tripbook
