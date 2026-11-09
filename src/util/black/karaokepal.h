#pragma once
#include <cstdint>
// The karaoke view's colours: the yellow deck's terminal look (termpal), so the two decks match.
// Black, in the theme the yellow deck shows (the car's INSIDE light's colour, pale by day and vivid at
// night; its green with the light off): the song in MID, the sung words and the played bar in TEXT,
// the rest in DIM. 0xRRGGBB. PURE, host-tested.
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

// DIM's share where it stands too close to TEXT (quiet() in the .cpp).
constexpr int QUIET_PCT = 55;

// c mixed into base at pct % (0..100), per channel, rounded.
uint32_t tint(uint32_t c, int pct, uint32_t base);
// The colours for a ledlook swatch (0..8): -1 or out of range is termpal's default (green); cyan
// (8), which termpal has no theme for, takes blue's, as on the yellow deck.
Pal of(int swatch, bool night);

}  // namespace karaokepal
