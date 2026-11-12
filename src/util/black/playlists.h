#pragma once
#include <cstddef>
// The watch's playlist picker on the black deck (net/remote's GET /playlists and /playlists/play,
// run by spotify/client on the network task). PURE, host-tested.
// GET /playlists?offset=N answers one page of Spotify's /v1/me/playlists as plain text:
//   playlists <total> <next>
//   <22-char id> <name>        (one line per playlist)
// next: the offset of the following page (offset + the items Spotify sent, null ones included); the
// watch asks for it while next < total. Names are cut to NAME_BYTES bytes (whole UTF-8 characters),
// control characters as spaces.
namespace playlists {
constexpr int PAGE = 8;            // asked of Spotify per page: ~1.5 KB of JSON each, in a 16 KB buffer
constexpr size_t ID_LEN = 22;      // a Spotify id: base62
constexpr size_t NAME_BYTES = 48;
constexpr int MAX_OFFSET = 500;    // a page further than this is refused (the watch stops well before)
constexpr size_t REPLY_MAX = 32 + PAGE * (ID_LEN + 1 + NAME_BYTES + 1);

// True for exactly 22 characters of A-Z a-z 0-9: nothing else goes into a path or a JSON body.
bool validId(const char* id);
// "12" -> 12; false for anything but 0..MAX_OFFSET in plain decimal digits.
bool parseOffset(const char* s, int* out);
// The reply above from a /v1/me/playlists body. Returns the number of playlists written, or -1 when
// the body has no items array or total, or out is too small (out is "" then).
int writePage(const char* body, int offset, char* out, size_t n);
// start-playback's body: {"context_uri":"spotify:playlist:<id>"}. False (out "") for a bad id or a
// short buffer.
bool playBody(const char* id, char* out, size_t n);
// The same, starting at track `position` of the playlist (0-based; Spotify's "offset"):
// {"context_uri":"spotify:playlist:<id>","offset":{"position":N}}. position < 0: playBody's body.
bool playBodyAt(const char* id, int position, char* out, size_t n);
// A playlist's track count from GET /v1/playlists/<id>?fields=items.total ({"items":{"total":N}};
// "tracks" before Spotify's February 2026 rename, still read).
bool readTotal(const char* body, int* total);
// Where to play when Spotify has no active device (start-playback's 404): from GET
// /v1/me/player/devices, the active one, else a phone with Spotify open, else any other; never a
// restricted one. False (out "") when there is none.
bool pickDevice(const char* body, char* out, size_t n);
// Where a shuffled start begins: random % total, or -1 (the first track, Spotify's own) when the
// total isn't known.
int startPosition(int total, unsigned random);

// What the watch is told about a Spotify answer that isn't the one wanted (code < 0: no answer).
// play: the answer was start-playback's (its 404 is "no active device"); body: its JSON or "".
// The deck sends it with HTTP 502, so the watch can tell it from its own errors (403: wrong key).
const char* why(int code, const char* body, bool play);
}
