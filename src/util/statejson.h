#pragma once
#include <cstddef>
#include <cstdint>
#include "lyricstatus.h"
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
    uint32_t trackGen;      // changes on every song
    uint32_t progressMs, durationMs;
    bool playing;
    uint32_t tripS;         // trip clock (util/tripclock)
    const char* lyricKind;  // lyricKindName()
    const char* line;       // current lyric line ("" during the intro or without lyrics)
    const char* next;       // the line after it
    uint32_t lineStartMs, nextStartMs;   // song position where they start (0 = unknown)
};
// "searching" (also while retrying), "synced", "plain", "instrumental", "none".
const char* lyricKindName(lyricstatus::Status s);
// Writes the JSON object into out (n bytes) and returns its length. When it doesn't fit,
// returns 0 and leaves "" (never half a document). Strings are escaped; UTF-8 passes through.
size_t write(const State& s, char* out, size_t n);
}
