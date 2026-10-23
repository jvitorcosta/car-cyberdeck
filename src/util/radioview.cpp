#include "radioview.h"
#include <cstdio>
#include <cstring>

namespace radioview {

int progressMs(const stateparse::Deck& d, uint32_t sincePollMs) {
    uint64_t p = d.progressMs;
    if (d.playing) p += sincePollMs;
    if (d.durationMs > 0 && p > d.durationMs) p = d.durationMs;
    if (p > 0x7FFFFFFF) p = 0x7FFFFFFF;
    return static_cast<int>(p);
}

void clock(uint32_t ms, char* buf, size_t n) {
    if (!buf || n == 0) return;
    const unsigned s = static_cast<unsigned>(ms / 1000);
    if (s >= 3600) snprintf(buf, n, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
    else snprintf(buf, n, "%u:%02u", s / 60, s % 60);
}

int cells(uint32_t pos, uint32_t dur, int n) {
    if (dur == 0 || n <= 0) return 0;
    if (pos >= dur) return n;
    return static_cast<int>(static_cast<uint64_t>(pos) * static_cast<uint64_t>(n) / dur);
}

const char* state(bool answered, bool wrongKey, bool playing, bool hasTrack) {
    if (wrongKey) return ">WRONG KEY";
    if (!answered) return ">NO SIGNAL";
    if (!playing || !hasTrack) return ">STANDBY";
    return "ON AIR";
}

const char* lyric(const stateparse::Deck& d, uint32_t posMs) {
    if (strcmp(d.lyricKind, "synced") != 0) return "";
    if (d.nextStartMs > 0 && posMs >= d.nextStartMs) return d.next;
    if (posMs >= d.lineStartMs) return d.line;
    return "";
}

}
