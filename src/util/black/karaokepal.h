#pragma once
#include <cstdint>
// The karaoke view's colours ("night drive, tinted by the light", spec 2026-11-09): dark in both
// modes, the car's INSIDE light's hue (termpal's TEXT, MID and DIM for that swatch, its pale tints by
// day and its vivid ones at night) on the song, the bar and the lines not sung yet, the sung words
// near white so they are the brightest thing on screen. 0xRRGGBB. PURE, host-tested.
namespace karaokepal {

struct Pal {
    uint32_t bg;       // the whole screen
    uint32_t song;     // the title
    uint32_t artist;
    uint32_t barOn;    // the played part of the progress bar
    uint32_t barOff;   // the rest of it
    uint32_t lit;      // the sung line's words once swept
    uint32_t unsung;   // the sung line's words before the sweep
    uint32_t prev;     // the line just sung
    uint32_t next;     // the next line
};

// The light's swatch when none came (or the light is off or has no link): white, neutral.
constexpr int DEFAULT_SWATCH = 6;

// c mixed into base at pct % (0..100), per channel, rounded.
uint32_t tint(uint32_t c, int pct, uint32_t base);
// The colours for a ledlook swatch (0..8): -1 or out of range is DEFAULT_SWATCH; cyan (8), which
// termpal has no theme for, takes blue's.
Pal of(int swatch, bool night);

}  // namespace karaokepal
