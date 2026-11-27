#pragma once
#include <Arduino_GFX_Library.h>
// The Pokédeck's picture: the Guition JC4827W543's 4.3" 480x272 NV3041A on QSPI. One task draws
// (the loop). Backlight: common/ui/backlight on PIN_BL, after begin().
namespace panel {
constexpr int W = 480, H = 272;
bool begin();   // false: the driver didn't start (don't draw)
Arduino_GFX& gfx();
}
