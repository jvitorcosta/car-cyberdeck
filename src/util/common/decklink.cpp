#include "decklink.h"

#include <cstdio>
#include <cstring>

namespace decklink {

const char* const LINKED_MSG = ">DECK: LINKED";
const char* const NOLINK_MSG = ">DECK: NO LINK";
const char* const FIX_MSG = ">GPS: FIX";
const char* const NOFIX_MSG = ">GPS: NO FIX";

bool linked(uint32_t heardAt, uint32_t now) { return heardAt != 0 && static_cast<uint32_t>(now - heardAt) < LINK_MS; }

void body(bool fix, char* out, size_t n) {
    if (n) snprintf(out, n, "gps=%c", fix ? '1' : '0');
}

Gps parse(const char* b) {
    if (!b || strncmp(b, "gps=", 4) != 0) return Gps::Unknown;
    const char v = b[4], after = b[5];
    if (after != '\0' && after != '\n' && after != '\r' && after != ' ') return Gps::Unknown;
    if (v == '1') return Gps::Fix;
    if (v == '0') return Gps::NoFix;
    return Gps::Unknown;
}

Gps shown(bool isLinked, Gps last) { return isLinked ? last : Gps::Unknown; }

bool isAlert(const char* msg) { return msg && (strcmp(msg, NOLINK_MSG) == 0 || strcmp(msg, NOFIX_MSG) == 0); }

const char* Tracker::update(bool isLinked, Gps g, uint32_t now, uint32_t notAnsweringAt) {
    const int8_t l = isLinked ? 1 : 0;
    if (l != link_) {
        const bool first = link_ < 0;
        link_ = l;
        if (!first) {
            if (l) return LINKED_MSG;
            if (notAnsweringAt != 0 && static_cast<uint32_t>(now - notAnsweringAt) < LINK_MS) return nullptr;
            return NOLINK_MSG;
        }
        // the first link state is silent; the GPS's first state is recorded (silently) below
    }
    if (!isLinked || g == Gps::Unknown) return nullptr;
    const int8_t f = g == Gps::Fix ? 1 : 0;
    if (f == gps_) return nullptr;
    const bool first = gps_ < 0;
    gps_ = f;
    if (first) return nullptr;
    return f ? FIX_MSG : NOFIX_MSG;
}

}  // namespace decklink
