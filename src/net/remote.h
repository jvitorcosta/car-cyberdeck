#pragma once
// Phone remote: a one-button web page on port 80 that plays a wolf howl on the deck.
// http://cardeck.local/?k=<REMOTE_TOKEN> (S3; cardeck-cyd.local on the CYD, REMOTE_NAME to
// change) or http://<deck-ip>/...; POST /play?clip=howl&k=... answers 202 (playing), 403
// (wrong key), 409 (a clip is playing) or 429 (< 3 s since the last howl); see util/remotegate.
// Off when REMOTE_TOKEN is empty. Runs on the UI loop (core 1); Spotify polling is untouched.
namespace remote {
void begin();   // after the boot scene; logs the address
void loop();    // once per UI frame
}
