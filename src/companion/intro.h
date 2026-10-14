#pragma once
#include <TFT_eSPI.h>
// The companion's boot intro video (data/intro.bin from tools/gen_intro.py; silent, embedded in
// this build only): played full screen by the clock on its own task while setup connects WiFi,
// then the home screen takes over. Without a clip everything here is a no-op.
namespace intro {
void start(TFT_eSPI& t);   // starts playing (no-op without a clip)
bool playing();
void wait();               // returns once the clip is over
}
