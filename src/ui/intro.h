#pragma once
#include <TFT_eSPI.h>
#include "../util/black/intropick.h"
// The S3's boot intro videos, one per video opening (util/intropick): the kiss (data/intro.bin +
// its sound data/intro.pcm) and Max Verstappen (data/max.bin + max.pcm), from tools/gen_intro.py,
// in the media partition. The picked one is played full screen by the clock on its own task before everything else, while setup
// connects WiFi and logs into Spotify; the boot terminal (ui/bootterm) types over it. If the deck
// isn't online when the clip ends, the last frame holds with the terminal (RETRYING...) until it is.
// Without a clip everything here is a no-op.
namespace intro {
bool available(intropick::Opening o);          // o's clip is in the build (kiss, max)
void start(TFT_eSPI& t, bool (*online)(), intropick::Opening o);   // no-op without its clip
bool playing();
void wait();               // returns once the clip is over and online() is true
}
