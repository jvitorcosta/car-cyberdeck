#pragma once
#include <TFT_eSPI.h>
// The S3's screen while an update arrives over WiFi (net/ota): a battle box whose EXP bar fills
// with the upload. full: the whole screen; else only the bar and its percent.
namespace ui {
void drawUpdate(TFT_eSPI& t, int pct, bool full);
}
