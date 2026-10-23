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

// "12.4 KM 0:32" body shared by line() and last(); only the prefix and the gap differ.
static void body(const Trip& t, const char* head, const char* gap, char* buf, size_t n) {
    const unsigned mins = t.seconds / 60;
    if (t.hasDistance) {
        const unsigned tenths = t.meters / 100;  // rounded down: a trip never reads further than it went
        snprintf(buf, n, "%s%u.%u KM%s%u:%02u", head, tenths / 10, tenths % 10, gap, mins / 60, mins % 60);
    } else {
        snprintf(buf, n, "%s-- KM%s%u:%02u", head, gap, mins / 60, mins % 60);
    }
}

void line(const Trip& t, char* buf, size_t n) { body(t, "TRIP ", "  ", buf, n); }
void last(const Trip& t, char* buf, size_t n) { body(t, ">LAST TRIP ", " ", buf, n); }

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
