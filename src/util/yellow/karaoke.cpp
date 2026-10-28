#include "karaoke.h"
#include <cstring>

namespace karaoke {

int progressMs(const stateparse::Deck& d, uint32_t sincePollMs) {
    uint64_t p = d.progressMs;
    if (d.playing) p += sincePollMs;
    if (d.durationMs > 0 && p > d.durationMs) p = d.durationMs;
    if (p > 0x7FFFFFFF) p = 0x7FFFFFFF;
    return static_cast<int>(p);
}

int cells(uint32_t pos, uint32_t dur, int n) {
    if (dur == 0 || n <= 0) return 0;
    if (pos >= dur) return n;
    return static_cast<int>(static_cast<uint64_t>(pos) * static_cast<uint64_t>(n) / dur);
}

bool available(bool answered, bool playing, const stateparse::Deck& d) {
    return answered && playing && strcmp(d.lyricKind, "synced") == 0 && (d.line[0] != 0 || d.nextStartMs > 0);
}

const char* state(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d) {
    if (wrongKey) return ">WRONG KEY";
    if (!answered) return ">LYRICS: NO SIGNAL";
    if (!playing || !d.track[0]) return ">NOTHING PLAYING";
    if (!available(answered, playing, d)) return ">NO SYNCED LYRICS";
    return "";
}

const char* whyNot(bool answered, bool wrongKey, bool playing, const stateparse::Deck& d) {
    if (wrongKey) return ">WRONG KEY";
    if (!answered) return ">LYRICS: NO SIGNAL";
    if (!playing || !d.track[0]) return ">NOTHING PLAYING";
    return ">NO SYNCED LYRICS";
}

bool isAlert(const char* msg) { return msg && strcmp(msg, ">WRONG KEY") == 0; }

const char* lyric(const stateparse::Deck& d, uint32_t posMs) {
    if (strcmp(d.lyricKind, "synced") != 0) return "";
    if (d.nextStartMs > 0 && posMs >= d.nextStartMs) return d.next;
    if (posMs >= d.lineStartMs) return d.line;
    return "";
}

const char* nextLine(const stateparse::Deck& d, uint32_t posMs) {
    if (strcmp(d.lyricKind, "synced") != 0) return "";
    if (d.nextStartMs > 0 && posMs >= d.nextStartMs) return "";
    if (posMs < d.lineStartMs) return d.line;
    return d.nextStartMs > 0 ? d.next : "";
}

}
