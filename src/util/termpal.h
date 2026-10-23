// The terminal look's palette ("Seegson", Alien: Isolation): pale green by day, amber at night, on black;
// one hue at three brightnesses, plus RED (ALERT) reserved for problems. Read at a glance in a moving
// car, so every role clears a WCAG contrast on black: 4.5:1 by day (DIM included), 3:1 at night (the
// backlight is dimmer then anyway). The day's darker greens carry little blue: the panel turns a
// blue-green to teal. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM, ALERT };

// Day: BG 0x000000, TEXT 0xAAFF8C (17.4:1), MID 0x5ABE28 (8.8:1), DIM 0x308A24 (4.8:1),
// ALERT 0xFF3C28 (5.9:1).
// Night (amber, picked on the panel): TEXT 0xFF6600 (7.2:1), MID 0xC84E00 (4.5:1), DIM 0xA03A00
// (3.1:1), ALERT 0xFF2A1A (5.6:1, a redder red), BG 0. The night greens sit well under red (at most
// 0.6x) so the amber reads warm, not olive, on the panel.
uint32_t rgb(Role r, bool night);
// The scanline colour drawn over the screen: day 0x030503, night 0x050300.
uint32_t scan(bool night);
// A colour tile on the screen (e.g. a swatch): day unchanged; night each channel x70/100.
uint32_t tile(uint32_t screenRgb, bool night);
// WCAG 2.x contrast of 0xRRGGBB on black: (L + 0.05) / 0.05, L its relative luminance; 1 .. 21.
double contrastOnBlack(uint32_t rgb);

}  // namespace termpal
