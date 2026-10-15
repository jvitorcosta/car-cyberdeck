#pragma once
// Spotify's currently-playing endpoint sometimes answers 204 ("nothing playing") for minutes while
// the iPhone plays; /me/player still reports playback. On such a 204 the deck asks /me/player and
// keeps the song it shows when that one says playing. PURE, host-tested.
namespace nowplaying {
enum class On204 { KeepTrack, Stopped };
// playerCode / playerPlaying: /me/player's answer; haveTrack: a song is known (something to keep).
inline On204 on204(int playerCode, bool playerPlaying, bool haveTrack) {
    return (playerCode == 200 && playerPlaying && haveTrack) ? On204::KeepTrack : On204::Stopped;
}
}
