#pragma once
#include <cstddef>
#include "lrcstream.h"
#include "../wire/lyricstatus.h"
// Which LRCLIB entry is this song, and what one attempt's requests add up to. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md.
namespace lrcmatch {
// The title without a suffix such as " - Remastered 2011", " (feat. X)", " [Live]" or " feat. X":
// LRCLIB's search wants every word it is given, and most entries have no suffix. A title that
// starts with one ("(I Can't Get No) Satisfaction") keeps it. Returns the length; nullptr -> "".
size_t baseTitle(const char* title, char* out, size_t n);
// Same song name, suffixes aside. Ignores case, Latin accents, punctuation and apostrophes;
// other scripts compare as they are. Search words match anywhere ("Love" finds "Love Story"), so
// the duration alone can pick another song. Either side with nothing to compare: true.
bool sameTitle(const char* a, const char* b);

struct Answer {
    int rc;               // HTTP status (negative: connect/TLS/timeout)
    bool stalled;         // the body stopped, or it wasn't JSON
    lrcstream::Kind kind;
};
// /api/get didn't settle it: nothing, plain only (its synced may be on another entry), or a server
// error (the search often answers while the exact lookup gets a 503).
bool needSearch(const Answer& get);
// The attempt's result. search is nullptr when none ran; with one, its kind includes the plain
// lines kept from /api/get. An attempt that couldn't look everywhere is a TempError, except that
// the last one shows the plain lines it has rather than nothing.
lyricstatus::Result settle(const Answer& get, const Answer* search, bool lastAttempt);
}
