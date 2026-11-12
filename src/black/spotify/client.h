#pragma once
#include <Arduino.h>
#include "app_state.h"
namespace spclient {
bool hasToken();                       // src/config.h has a refresh token (tools/spotify_auth.py)
void begin();                          // first access token; then every request refreshes it as needed
// One GET /v1/me/player: track, progress, status, context name, device, volume, shuffle, repeat.
void poll(AppState& st);
// The song is playing from a playlist (its name is st.context), and that playlist's description
// (raw UTF-8, HTML stripped, at most 159 bytes; "" when none or not from a playlist). Both as of the
// last poll; network task.
bool fromPlaylist();
const char* playlistDescription();
// The watch's playlist picker (net/remote, util/black/playlists). Called from the remote task, they
// hand the request to the network task, where the connection and the answer buffer live, and wait
// for it. The HTTP status for the watch, its text in out: 200 with the page or "playing"; 502 with
// what Spotify's answer means (playlists::why); 503 busy or low on memory; 504 not run in time.
int watchPlaylists(int offset, char* out, size_t n);
// shuffle: from a random track with Spotify's shuffle on (the yellow deck's IR remote); without it
// from the first, shuffle as it was (the watch).
int watchPlay(const char* id, bool shuffle, char* out, size_t n);
// Network task, each pass: runs the watch's request if one waits. rateLimited: polls are waiting out a 429.
void serveWatch(bool rateLimited);
}
