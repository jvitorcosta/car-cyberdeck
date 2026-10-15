#pragma once
#include <cstdint>
#include <cstring>
#include "stateparse.h"
// What the companion's lyric area shows between two polls: the big line and the small dim one.
// The CYD switches to the next line on its own clock (progress + time since the poll), so line
// changes land on time although it only polls once a second. PURE, host-tested.
namespace lyricclock {
struct View { const char* big; const char* small; };

inline View pick(const stateparse::Deck& d, uint32_t sincePollMs) {
    const bool lyrics = strcmp(d.lyricKind, "synced") == 0 || strcmp(d.lyricKind, "plain") == 0;
    if (lyrics) {
        const uint32_t pos = d.progressMs + (d.playing ? sincePollMs : 0);
        if (d.next[0] && d.nextStartMs && pos >= d.nextStartMs) return {d.next, ""};
        if (d.line[0]) return {d.line, d.next};
        return {d.track, d.next};                 // intro: title big, the first line coming up
    }
    return {d.track, d.artist};                   // searching / none / instrumental
}
}
