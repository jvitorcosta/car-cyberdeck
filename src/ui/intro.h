#pragma once
#include <TFT_eSPI.h>
// The S3's boot intro video (data/intro.bin + its sound data/intro.pcm, from tools/gen_intro.py;
// embedded in the s3 build only): the kiss opening, one of the random boot openings
// (util/intropick), played full screen by the clock on its own task while setup connects WiFi and
// logs into Spotify; the boot terminal (ui/bootterm) types over it. If the deck
// isn't online when the clip ends, the last frame holds with the terminal (RETRYING...) until it is.
// Without a clip (or on the CYD) everything here is a no-op.
namespace intro {
bool available();                              // a clip is in the build
void start(TFT_eSPI& t, bool (*online)());     // starts playing (no-op without a clip)
bool playing();
void wait();               // returns once the clip is over and online() is true
}
