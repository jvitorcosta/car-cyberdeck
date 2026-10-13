#pragma once
#include <cstddef>
#include <cstdint>
// The deck's state as JSON for GET /state (net/remote): the phone page and, later, the side
// screen read it. PURE, host-tested.
namespace statejson {
struct State {
    const char* track;    // nullptr = ""
    const char* artist;
    const char* title;    // status screen title ("" while the deck shows the song)
    const char* detail;   // status screen detail line
    const char* ssid;     // connected network
    int rssi;             // dBm
    uint32_t uptimeS;     // seconds since power-on (the trip, in a car)
    const char* const* clips;   // sound buttons the remote accepts ("howl", ...)
    int clipCount;
};
// Writes the JSON object into out (n bytes) and returns its length. When it doesn't fit,
// returns 0 and leaves "" (never half a document). Strings are escaped; UTF-8 passes through.
size_t write(const State& s, char* out, size_t n);
}
