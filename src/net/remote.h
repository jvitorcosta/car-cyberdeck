#pragma once
#include <cstddef>
// Phone remote: a one-button web page on port 80 that plays a wolf howl on the deck.
// http://cardeck.local/?k=<REMOTE_TOKEN> (S3; cardeck-cyd.local on the CYD, REMOTE_NAME to
// change) or http://<deck-ip>/...; POST /play?clip=howl&k=... answers 202 (playing), 403
// (wrong key), 409 (a clip is playing), 429 (< 3 s since the last howl; see util/remotegate)
// or 503 (heap too low). Off when REMOTE_TOKEN is empty. Runs in its own idle-priority task
// (4 KB stack), so a slow client never stalls the display; Spotify polling is untouched.
namespace remote {
// Starts the task; it serves once `hold` is false (the boot scene) and WiFi is up, then logs
// the address.
void begin(bool (*hold)());
// The status screen's text for GET /state ("" while the deck shows the song). Any task.
void setStatus(const char* title, const char* detail);
}
