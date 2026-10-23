#pragma once
// Where the yellow deck's car-LED panel (ledpanel.cpp) puts its text, as far as fitting goes: the
// x of each fixed label in its rect, the header lines, the Menu strip's columns and the lyric
// pitches. Shared with test_termfont, which checks every fixed label fits at its termfont Face with
// room to spare. PURE, host-tested.
#include "termfont.h"

namespace termlayout {

// Menu rows ("> [LIGHTS]", BODY) and preset blocks (the name in TITLE, left of the two chips that
// start PRESET_CHIPS_FROM_RIGHT px from the block's right edge): text x from the rect's left.
constexpr int ROW_TEXT_X = 8;
constexpr int PRESET_TEXT_X = 8, PRESET_CHIPS_FROM_RIGHT = 50;

// A Lights colour button (75x44): the chip left, as tall as before but 20 wide (was 28 square) so the
// name (SMALL) fits right of it.
constexpr int TILE_CHIP_X = 5, TILE_CHIP_W = 20, TILE_CHIP_H = 28, TILE_TEXT_X = 29;

// The Lights status line (SMALL, in deckmenu::STATUS), the Weather body's left edge and its title tab.
constexpr int STATUS_TEXT_X = 4;
constexpr int WEATHER_X = 16, WEATHER_TAB_W = 100;

// The Menu header: three SMALL lines, caps tops HEADER_Y0 + i * HEADER_DY, above the rule at y 41.
constexpr const char* HEADER_LINES[] = {"WELCOME TO ROBCO INDUSTRIES (TM) TERMLINK", ">SET TERMINAL/INQUIRE",
                                        "HONDA CITY // CARDECK OS"};
constexpr int N_HEADER = sizeof(HEADER_LINES) / sizeof(HEADER_LINES[0]);
constexpr int HEADER_X = 8, HEADER_Y0 = 4, HEADER_DY = 12;

// The Menu strip: the clock (BIG) from STRIP_CLOCK_X with the temperature under its right end, then
// the text lines (SMALL) from STRIP_TEXT_X to STRIP_RIGHT. On the lights' line a lit light's chip
// (STRIP_CHIP square) takes the space before its colour name, in a STRIP_CHIP_SLOT px slot; the two
// lights are STRIP_LIGHT_GAP apart.
constexpr int STRIP_CLOCK_X = 6, STRIP_TEXT_X = 84, STRIP_RIGHT = 316;
constexpr int STRIP_CHIP = 7, STRIP_CHIP_SLOT = 9, STRIP_LIGHT_GAP = 12;

// One light's half of lightstatus::strip's line ("IN" + "PINK 40%"), where it goes.
struct StripPart {
    const char* name;   // "IN" / "OUT"
    const char* state;  // "PINK 40%", "OFF", "--"; nullptr when the half had none
    int nameX, chipX, stateX;  // chipX -1: no chip (a space instead)
};
// Splits lightstatus::strip's line ("IN <state>  OUT <state>") in place into its two halves laid out
// from x0, a chip before the state of each half whose chip[i] is set. Returns where the text ends
// (the cursor's x), or -1 when the line isn't two halves (the caller draws it plain).
int stripLayout(char* line, int x0, const bool chip[2], StripPart out[2]);

// The KARAOKE body: the line due now in BIG over up to 3 lines KARAOKE_BIG_DY apart (or, when that
// would cut it, TITLE over up to 4 lines KARAOKE_FIT_DY apart) in KARAOKE_BIG_H rows; the next line in
// BODY over up to 2 lines KARAOKE_NEXT_DY apart in KARAOKE_NEXT_H rows; KARAOKE_W px wide.
constexpr int KARAOKE_W = 312;
constexpr int KARAOKE_BIG_H = 90, KARAOKE_BIG_DY = 30, KARAOKE_FIT_DY = 22;
constexpr int KARAOKE_NEXT_H = 40, KARAOKE_NEXT_DY = 20;

}  // namespace termlayout
