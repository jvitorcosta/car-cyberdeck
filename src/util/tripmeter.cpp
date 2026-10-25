#include "tripmeter.h"
#include <cstdio>

namespace tripmeter {

void Meter::addSpeed(uint16_t kmh, uint32_t dtMs) {
    hasDistance_ = true;
    // 64-bit: 65535 km/h * a long gap would overflow 32 bits before the divide.
    const uint64_t total = static_cast<uint64_t>(kmh) * dtMs + rem_;
    meters_ += static_cast<uint32_t>(total / 3600);
    rem_ = static_cast<uint32_t>(total % 3600);
}

Trip Meter::trip() const {
    Trip t;
    t.seconds = seconds_;
    t.meters = meters_;
    t.hasDistance = hasDistance_;
    return t;
}

void elapsed(const Trip& t, char* buf, size_t n) {
    const unsigned mins = t.seconds / 60;
    snprintf(buf, n, "%u:%02u", mins / 60, mins % 60);
}

// "12.4", rounded down: a trip never reads further than it went.
static void km(const Trip& t, char* buf, size_t n) {
    const unsigned tenths = t.meters / 100;
    snprintf(buf, n, "%u.%u", tenths / 10, tenths % 10);
}

void distance(const Trip& t, char* buf, size_t n) {
    if (!t.hasDistance) {
        snprintf(buf, n, "-- NO LINK");
        return;
    }
    char k[16];
    km(t, k, sizeof(k));
    snprintf(buf, n, "%s KM", k);
}

void last(const Trip& t, char* buf, size_t n) {
    char k[16], hm[16];
    if (t.hasDistance) km(t, k, sizeof(k));
    else snprintf(k, sizeof(k), "--");
    elapsed(t, hm, sizeof(hm));
    snprintf(buf, n, ">LAST TRIP %s KM %s", k, hm);
}

void encode(const Trip& t, uint8_t out[RECORD_BYTES]) {
    for (int i = 0; i < 4; i++) {
        out[i] = static_cast<uint8_t>(t.seconds >> (8 * i));
        out[4 + i] = static_cast<uint8_t>(t.meters >> (8 * i));
    }
    out[8] = t.hasDistance ? 1 : 0;
}

bool decode(const uint8_t* in, size_t len, Trip* t) {
    *t = Trip{};
    if (len != RECORD_BYTES || in[8] > 1) return false;
    for (int i = 0; i < 4; i++) {
        t->seconds |= static_cast<uint32_t>(in[i]) << (8 * i);
        t->meters |= static_cast<uint32_t>(in[4 + i]) << (8 * i);
    }
    t->hasDistance = in[8] == 1;
    return true;
}

}  // namespace tripmeter
