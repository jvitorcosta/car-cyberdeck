#pragma once
#include <TFT_eSPI.h>
// The S3's boot intro video (data/intro.bin + its sound data/intro.pcm, from tools/gen_intro.py;
// embedded in the s3 build only): played full screen by the clock on its own task before everything else, while setup
// connects WiFi and logs into Spotify; then the night drive takes over. Without a clip (or on the
// CYD) everything here is a no-op.
namespace intro {
void start(TFT_eSPI& t);   // starts playing (no-op without a clip)
bool playing();
void wait();               // returns once the clip is over
}
