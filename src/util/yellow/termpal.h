// The terminal look's palette ("Seegson", Alien: Isolation): pale green on black by day, a vivid
// phosphor green at night; one hue at three brightnesses, plus RED (ALERT) reserved for problems. Read
// at a glance in a moving car: by day every role clears a WCAG contrast on black of 4.5:1 (DIM
// included); at night (under the deck's night backlight) TEXT 3:1 and DIM 2:1, since a fully saturated
// red or blue can't reach more. The darker greens carry little blue: the panel turns a blue-green to
// teal. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM, ALERT };

// Day: BG 0x000000, TEXT 0xAAFF8C (17.4:1), MID 0x5ABE28 (8.8:1), DIM 0x308A24 (4.8:1),
// ALERT 0xFF3C28 (5.9:1).
// Night: the green theme's night colours below (a phosphor-terminal green).
uint32_t rgb(Role r, bool night);
// The same roles in a theme: the colour of the car's INSIDE light, `theme` a ledlook swatch index
// (0..7: red, orange, pink, purple, blue, green, white, yellow); -1 (or anything out of range) is the
// default above. Each theme is its hue at three brightnesses (day TEXT >= 7:1, MID and DIM >= 4.5:1).
// By day blue and purple are lightened so they read, green is the default green, white a neutral
// grey-white. At night each theme keeps its own hue at full saturation so it stays vivid on the
// dimmed panel (the day's pale tints go grey there; no amber override), picked by eye on the panel on
// 2026-11-05 (night TEXT >= 3:1, DIM >= 2:1). The panel pushes green: the night red, orange and pink
// carry none or little (or they turn orange or yellow), the pink little blue (or it turns purple),
// and a darker yellow reads olive, so the night yellow is one colour at all three levels. ALERT is
// white on red, orange and pink, where a red would not stand out, else red (day the default red,
// night a pure red); it is always a channel 0x60 or more from TEXT. Contrast on black,
// TEXT / MID / DIM / ALERT:
//   red     day FF8C78 9.3  FF4A3A 6.3  F03424 5.2  FFFFFF 21    night FF0000 5.3  E00000 4.2  BC0000 3.1  FFFFFF 21
//   orange  day FFAA64 11.2 F57814 7.6  DC6400 5.9  FFFFFF 21    night FF4600 6.1  E03400 4.7  BC2800 3.4  FFFFFF 21
//   pink    day FF8CCD 9.9  FF50B4 7.1  F0379E 5.8  FFFFFF 21    night FF0040 5.3  E00038 4.2  BC0030 3.2  FFFFFF 21
//   purple  day D2B4FF 11.7 B48CFF 8.1  9A6EFA 6.0  FF3C28 5.9   night D200FF 5.2  B800E0 4.1  9A00BC 3.1  FF0000 5.3
//   blue    day A0C8FF 12.2 78A5FF 8.6  5A87F5 6.2  FF3C28 5.9   night 0046FF 3.3  003CE0 2.7  0032BC 2.2  FF0000 5.3
//   green   day AAFF8C 17.4 5ABE28 8.8  308A24 4.8  FF3C28 5.9   night 50FF46 15.7 46E03C 12.0 3ABC32 8.4  FF0000 5.3
//   white   day F0F0E6 18.3 B4B4AA 10.1 8C8C84 6.2  FF3C28 5.9   night FFFFFF 21   EBEBEB 17.6 D2D2D2 13.9 FF0000 5.3
//   yellow  day FFDC5A 15.6 FF9900 9.8  D27E00 6.7  FF3C28 5.9   night FFB400 11.8 FFB400 11.8 FFB400 11.8 FF0000 5.3
uint32_t rgb(Role r, bool night, int theme);
// The scanline colour drawn over the screen: 0x030503, day and night (no amber tint).
uint32_t scan(bool night);
// A colour tile on the screen (e.g. a swatch): day unchanged; night each channel x70/100.
uint32_t tile(uint32_t screenRgb, bool night);
// WCAG 2.x contrast of 0xRRGGBB on black: (L + 0.05) / 0.05, L its relative luminance; 1 .. 21.
double contrastOnBlack(uint32_t rgb);

}  // namespace termpal
