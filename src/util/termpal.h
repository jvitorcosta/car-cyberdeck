// The terminal look's palette ("Seegson", Alien: Isolation): pale green by day, amber at night, on black;
// one hue at three brightnesses, plus RED (ALERT) reserved for problems. PURE,
// host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM, ALERT };

// Day: BG 0x000000, TEXT 0x96FFAA, MID 0x50AA64, DIM 0x235530, ALERT 0xFF3C28.
// Night (amber): TEXT 0xB37B00, MID 0x6E4C00, DIM 0x372600, ALERT 0xB02A10 (a deeper red), BG 0.
uint32_t rgb(Role r, bool night);
// The scanline colour drawn over the screen: day 0x061008, night 0x0A0700.
uint32_t scan(bool night);
// A colour tile on the screen (e.g. a swatch): day unchanged; night each channel x70/100.
uint32_t tile(uint32_t screenRgb, bool night);

}  // namespace termpal
