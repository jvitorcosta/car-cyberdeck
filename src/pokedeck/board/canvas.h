#pragma once
#include <TFT_eSPI.h>
// The black deck's screens draw with TFT_eSPI, which doesn't drive this panel. Here they draw into
// a 480x272 sprite in PSRAM instead (their TFT_eSPI& is the sprite), and present() sends it to the
// panel. The loop task draws and presents.
namespace canvas {
bool begin();         // after panel::begin(); false: no memory for the frame
TFT_eSPI& screen();   // what the screens draw on
void present();       // the whole frame to the panel
}
