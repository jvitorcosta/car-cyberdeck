#pragma once
#include <TFT_eSPI.h>
#include "../../util/car/obdwire.h"
// The gauge screen's Suki look: the film monitor from 2 Fast 2 Furious as the background (the art
// is a file on the deck, data/suki.bin from tools/gen_suki_hud.py, git-ignored), the live values in
// its own places. The waveform window: the last 12 s of RPM as the film's pink capsules. The FD / HD
// lines: battery, throttle, coolant, intake. The clock. The three STAGE bars: the 0-100 timer (it
// counts while a run is on), the best 0-100 this power-on and km/l now. The window under the film:
// the speed in capsule digits, a 20-capsule tach (red from 6000) with the RPM, and the load capsule
// (red from 85 %). Off the link the RPM's place says OFF / SEARCH / KEY ON. The layout and the
// drawing are util/car/sukihud's; whole: the background and everything, else only what changed.
// UI loop only. Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Suki look")
namespace ui {
bool sukiReady();   // the art is on the deck (read once): else the street look (gaugeview)
void drawSuki(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole);
}
