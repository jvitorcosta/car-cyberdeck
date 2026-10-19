#pragma once
#include <TFT_eSPI.h>
#include <cstdint>
#include "../util/bios.h"
// The on-board terminal: the BIOS log carried on in the boot scene's sky (ui/bootscene) and over
// the intro video (ui/intro). The script and the live lines (WI-FI, ENGINE, GREETING, PARKING) come
// from util/bios; this draws them, straight on the display or into a sprite.
namespace bootterm {
constexpr int X = 4, Y = 4, ROW = 10;                       // top left; 10 px a row
constexpr int W = (bios::TERM_COLS + 2) * 6 + 2;           // 206
constexpr int H = bios::TERM_ROWS * ROW;                    // 60
void begin(bool (*online)());       // fresh terminal for a new scene (clears every line and flag)
void setCaption(const char* text);  // static string; "retry" in it turns WI-FI to RETRYING...
const char* caption();
uint32_t captionGen();              // bumps on every setCaption
void events(uint32_t t, bool braking);   // feeds the script and the live lines due by t
uint32_t gen();                     // bumps when any line changed (redraw)
// Rows straight on the display, redrawn only when they change; bg(y) repaints a row's background.
void drawDirect(TFT_eSPI& tft, uint32_t t, uint16_t (*bg)(int y));
// Rows into a sprite whose (0,0) is screen (x0, y0): a strip of the drive, or the video overlay.
void drawInto(TFT_eSprite& spr, uint32_t t, int x0, int y0);
}
