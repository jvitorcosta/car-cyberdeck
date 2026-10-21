#pragma once
#include <cstddef>
// The companion's GET /status for its phone page: the car link, the speed, the view pinned and
// the one on screen, and the line she shows (her line, else the lyric). PURE, host-tested.
namespace carstatus {
constexpr size_t MAX_JSON = 512;   // every field at its longest, escaped (test_carstatus)
struct Status {
    const char* car;    // "live" "noengine" "connecting" "off"
    int kmh;            // -1: unknown
    const char* pin;    // driveview::name(Pin)
    const char* show;   // driveview::name(Show)
    const char* line;   // nullptr = ""; cut to MAX_LINE bytes, never mid-character
};
constexpr size_t MAX_LINE = 160;
// {"car":"live","kmh":87,"pin":"auto","show":"drive","line":"..."}; false if it didn't fit in n.
bool write(const Status& s, char* out, size_t n);
}
