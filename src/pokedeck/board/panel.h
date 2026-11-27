#pragma once
#include <cstdint>
// The Pokédeck's picture: the Guition JC4827W543's 4.3" 480x272 NV3041A on QSPI. One task draws
// (the loop). Backlight: common/ui/backlight on PIN_BL, after begin().
namespace panel {
constexpr int W = 480, H = 272;
bool begin();   // false: the driver didn't start (don't draw)
// A whole W x H frame of big-endian RGB565. For single shapes see board/panelgfx.h; it can't be
// included beside TFT_eSPI (their driver macros collide).
void push(uint16_t* bigEndian);
}
