#pragma once
#include <TFT_eSPI.h>
#include "../net/ota.h"
// The S3's screen while an update arrives over WiFi (net/ota).
namespace ui {
// The battle box whose EXP bar fills with the upload (without the Green Hill Zone file). full:
// the whole screen; else only the bar and its percent.
void drawUpdate(TFT_eSPI& t, int pct, bool full);
// Green Hill Zone (util/backdrop, spec 2026-10-22-shadow-update-screen-design): the
// red rail filling, Shadow skating its tip (util/ridersprite; two jet glows without his file),
// standing still on Failed, a red flash on Done,
// the grey rail and "Update failed" on Failed. Repaints only what changed (full: everything).
// False, drawing nothing, without the backdrop file.
bool drawShadowUpdate(TFT_eSPI& t, const ota::View& v, uint32_t nowMs, bool full);
}
