// The terminal look's palette ("Seegson", Alien: Isolation): pale green on black; one hue at three
// brightnesses, plus RED (ALERT) reserved for problems. Read at a glance in a moving car, so every role
// clears a WCAG contrast of 4.5:1 on black (DIM included). The darker greens carry little blue: the
// panel turns a blue-green to teal. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM, ALERT };

// BG 0x000000, TEXT 0xAAFF8C (17.4:1), MID 0x5ABE28 (8.8:1), DIM 0x308A24 (4.8:1), ALERT 0xFF3C28 (5.9:1).
uint32_t rgb(Role r);
// The same roles in a theme: the colour of the car's INSIDE light, `theme` a ledlook swatch index
// (0..7: red, orange, pink, purple, blue, green, white, yellow); -1 (or anything out of range) is the
// default above. Each theme is its hue at three brightnesses with the same rules as the default (TEXT
// >= 7:1, MID and DIM >= 4.5:1). Blue and purple are lightened so they read; green is the default
// green; white is a neutral grey-white, held back so it doesn't glare. ALERT is white on red, orange
// and pink, where a red would not stand out, else the default red; it is always a channel 0x60 or
// more from TEXT. Contrast on black, TEXT / MID / DIM / ALERT:
//   red     FF8C78 9.3  FF4A3A 6.3  F03424 5.2  FFFFFF 21
//   orange  FFAA64 11.2 F57814 7.6  DC6400 5.9  FFFFFF 21
//   pink    FF8CCD 9.9  FF50B4 7.1  F0379E 5.8  FFFFFF 21
//   purple  D2B4FF 11.7 B48CFF 8.1  9A6EFA 6.0  FF3C28 5.9
//   blue    A0C8FF 12.2 78A5FF 8.6  5A87F5 6.2  FF3C28 5.9
//   green   AAFF8C 17.4 5ABE28 8.8  308A24 4.8  FF3C28 5.9
//   white   D2D2C8 13.8 9C9C94 7.6  7C7C74 5.0  FF3C28 5.9
//   yellow  FFDC5A 15.6 FF9900 9.8  D27E00 6.7  FF3C28 5.9
uint32_t rgb(Role r, int theme);
// The scanline colour drawn over the screen.
uint32_t scan();
// WCAG 2.x contrast of 0xRRGGBB on black: (L + 0.05) / 0.05, L its relative luminance; 1 .. 21.
double contrastOnBlack(uint32_t rgb);

}  // namespace termpal
