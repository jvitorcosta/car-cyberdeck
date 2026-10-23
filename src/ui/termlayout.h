#pragma once
// Where the yellow deck's car-LED panel (ledpanel.cpp) puts its text, as far as fitting goes: the
// x of each fixed label in its rect, the boot's typed lines, the dashboard's bands and the lyric
// pitches. Shared with test_termfont, which checks every fixed label fits at its termfont Face with
// room to spare. PURE, host-tested.
#include "termfont.h"

namespace termlayout {

// The Menu's items ("> [LIGHTS]", BODY): text x from the rect's left.
constexpr int ROW_TEXT_X = 8;
// KARAOKE's LIVE tag while it opens (SMALL, an inverted mini-label): LIVE_PAD px of fill around the
// caps, its right edge LIVE_RIGHT in from the item's.
constexpr const char* LIVE_TAG = "LIVE";
constexpr int LIVE_PAD = 3, LIVE_RIGHT = 6;

// A Lights colour button (75x44): the chip left, 16x28, so the name fits right of it in BODY, the
// preset names' face ("ORANGE", "PURPLE", "YELLOW" are the widest).
constexpr int TILE_CHIP_X = 4, TILE_CHIP_W = 16, TILE_CHIP_H = 28, TILE_TEXT_X = 24;

// A Lights preset (75x36): the name (BODY, caps top PRESET_NAME_Y) centred over two PRESET_CHIP px
// square chips (inside, outside) PRESET_CHIP_GAP apart, their top PRESET_CHIP_Y; all from the rect's
// top / centre.
constexpr int PRESET_NAME_Y = 4, PRESET_CHIP = 12, PRESET_CHIP_GAP = 6, PRESET_CHIP_Y = 20;

// The Lights status line (SMALL, in deckmenu::STATUS).
constexpr int STATUS_TEXT_X = 4;

// The boot: three SMALL lines typed out on a blank screen, caps tops BOOT_Y0 + i * BOOT_DY. A line's
// pitch is its caps and its descent ('Q', ','), so no descender lands on the next line's caps.
constexpr const char* BOOT_LINES[] = {"WELCOME TO CYBERDECK", ">SET TERMINAL/INQUIRE",
                                      "HONDA CITY // CARDECK OS"};
constexpr int N_BOOT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
constexpr int BOOT_X = 8, BOOT_Y0 = 2, BOOT_DY = 13;

// ---- The Menu: a dashboard of stacked bands (deckmenu's HEADER, CLOCK_BAND, LIGHTS_BAND, the items,
// MENU_STRIP), each under a MID rule from x 4 to 315. Text starts at DASH_X; right-aligned text ends
// at DASH_RIGHT. A _Y is the caps' top row, a _BASE the baseline (the row under the caps), as
// ledpanel's text() places them.
constexpr int DASH_X = 8, DASH_RIGHT = 312;
// the header: one line (SMALL) and its rule
constexpr const char* DASH_TITLE = "CYBERDECK // HONDA CITY";
constexpr int DASH_TITLE_Y = 5, HEADER_RULE_Y = 20;
// the clock (BIG) with the date (SMALL) under it left of DASH_SPLIT; the temperature (BIG) with the
// sky in words (SMALL) under it right of it, right-aligned; then the HI / LO line (SMALL) across
constexpr int DASH_SPLIT = 160;
constexpr int CLOCK_Y = 29, DATE_Y = 58, WORDS_Y = 58, HILO_Y = 74, CLOCK_RULE_Y = 90;
// the lights: the label (SMALL), then a row per light LIGHT_ROW_DY apart: its name (BODY), a chip of
// its colour as tall as TITLE's caps, the colour name and the level right-aligned (TITLE), all on
// one baseline
constexpr int LIGHTS_LABEL_Y = 96, LIGHT_ROW_BASE = 125, LIGHT_ROW_DY = 22, LIGHTS_RULE_Y = 156;
constexpr int LIGHT_CHIP_X = 84, LIGHT_CHIP_W = 18, LIGHT_CHIP_H = 14, LIGHT_NAME_X = 108;
// the strip under the items: the trip line, then the message line (empty unless a message shows);
// the block cursor (a SMALL cell) ends the trip line, or the message while one shows, by DASH_RIGHT
constexpr int STRIP_RULE_Y = 204, TRIP_BASE = 222, MSG_BASE = 237;

// The KARAOKE body: the line due now in BIG over up to 3 lines KARAOKE_BIG_DY apart (or, when that
// would cut it, TITLE over up to 4 lines KARAOKE_FIT_DY apart) in KARAOKE_BIG_H rows; the next line in
// BODY over up to 2 lines KARAOKE_NEXT_DY apart in KARAOKE_NEXT_H rows; KARAOKE_W px wide.
constexpr int KARAOKE_W = 312;
constexpr int KARAOKE_BIG_H = 90, KARAOKE_BIG_DY = 30, KARAOKE_FIT_DY = 22;
constexpr int KARAOKE_NEXT_H = 40, KARAOKE_NEXT_DY = 20;

}  // namespace termlayout
