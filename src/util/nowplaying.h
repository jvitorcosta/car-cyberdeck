#pragma once
// Spotify's currently-playing endpoint sometimes answers 204 ("nothing playing") for minutes while
// the iPhone plays; /me/player still reports playback. On such a 204 the deck asks /me/player and
// keeps the song it shows when that one says playing. PURE, host-tested.
namespace nowplaying {
// Unknown: /me/player itself failed (TLS error, timeout, 5xx), so it can't say either way; the
// deck keeps what it shows instead of dropping to "Nothing playing" on a bogus 204.
enum class On204 { KeepTrack, Stopped, Unknown };
// playerCode / playerPlaying: /me/player's answer; haveTrack: a song is known (something to keep).
inline On204 on204(int playerCode, bool playerPlaying, bool haveTrack) {
    if (!haveTrack) return On204::Stopped;
    if (playerCode <= 0 || playerCode >= 500) return On204::Unknown;
    return (playerCode == 200 && playerPlaying) ? On204::KeepTrack : On204::Stopped;
}
}
