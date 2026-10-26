#pragma once
// Where the yellow deck's car-LED panel (ledpanel.cpp) puts its text, as far as fitting goes: the
// x of each fixed label in its rect, the boot's typed lines, the dashboard's bands and the lyric
// pitches. Shared with test_termfont, which checks every fixed label fits at its termfont Face with
// room to spare. PURE, host-tested.
#include "termfont.h"

namespace termlayout {

// The Menu's three items (100 px wide), in deckmenu::Item order: the label (BODY) top left, its caps'
// top ROW_TEXT_Y and its x ROW_TEXT_X from the rect's; under it, right-aligned, the item's tag while
// it has one (SMALL, an inverted mini-label: TAG_PAD px of fill around the caps, TAG_RIGHT in from
// the item's right edge, TAG_BOTTOM up from its bottom): LYRICS's LIVE while it opens, MUSIC MODE's ON
// or OFF (always one of them). "[LYRICS]" and LIVE side by side don't fit 100 px, so the tag goes under
// the label, and the label always sits high (it doesn't jump when a tag comes).
constexpr const char* MENU_ITEMS[] = {"[LIGHTS]", "[LYRICS]", "MUSIC MODE"};
constexpr int N_MENU_ITEMS = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
constexpr int ROW_TEXT_X = 8, ROW_TEXT_Y = 7;
constexpr const char* LIVE_TAG = "LIVE";
constexpr const char* ON_TAG = "ON";
constexpr const char* OFF_TAG = "OFF";
constexpr int TAG_PAD = 3, TAG_RIGHT = 6, TAG_BOTTOM = 5;

// Lights' tabs (deckmenu::Tab order), in SMALL: four 56 px tabs fit beside < BACK, and BODY's
// "PRESETS" (55 px) wouldn't fit one with the margins.
constexpr const char* TAB_LABELS[] = {"BOTH", "INSIDE", "OUTSIDE", "PRESETS"};
constexpr int N_TAB_LABELS = sizeof(TAB_LABELS) / sizeof(TAB_LABELS[0]);

// A Lights colour button (75x64): the chip left, 16x48, so the name fits right of it in BODY
// ("ORANGE", "PURPLE", "YELLOW" are the widest).
constexpr int TILE_CHIP_X = 4, TILE_CHIP_W = 16, TILE_CHIP_H = 48, TILE_TEXT_X = 24;

// A preset on the PRESETS tab (100x60): the name (TITLE, caps top PRESET_NAME_Y) centred over two
// PRESET_CHIP px square chips (inside, outside) PRESET_CHIP_GAP apart, their top PRESET_CHIP_Y; all
// from the rect's top / centre.
constexpr int PRESET_NAME_Y = 9, PRESET_CHIP = 18, PRESET_CHIP_GAP = 10, PRESET_CHIP_Y = 33;

// The Lights status line (SMALL, in deckmenu::STATUS).
constexpr int STATUS_TEXT_X = 4;

// The boot: three SMALL lines typed out on a blank screen, caps tops BOOT_Y0 + i * BOOT_DY. A line's
// pitch is its caps and its descent ('Q', ','), so no descender lands on the next line's caps.
constexpr const char* BOOT_LINES[] = {"WELCOME TO CYBERDECK", ">SET TERMINAL/INQUIRE",
                                      "HONDA CITY // CARDECK OS"};
constexpr int N_BOOT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
constexpr int BOOT_X = 8, BOOT_Y0 = 2, BOOT_DY = 13;

// ---- The Menu: a dashboard of stacked bands (deckmenu's HEADER, CLOCK_BAND, TRIP_BAND, LIGHTS_BAND,
// PROMPT_BAND, then the items at the bottom), each band but the prompt line under a MID rule from x 4
// to 315 (from the clock band down, the next band's first row). Text starts at DASH_X; right-aligned
// text ends at DASH_RIGHT. A _Y is the caps' top row, a _BASE the baseline (the row under the caps),
// as ledpanel's text() places them.
constexpr int DASH_X = 8, DASH_RIGHT = 312;
// the header: one line (SMALL) and its rule
constexpr const char* DASH_TITLE = "CYBERDECK // HONDA CITY TOURING 2022";
constexpr int DASH_TITLE_Y = 5, HEADER_RULE_Y = 20;
// the clock (BIG) with the date (SMALL) under it left of DASH_SPLIT; the temperature (BIG) with the
// sky in words (SMALL) under it right of it, right-aligned; then the band's last row, on HILO_BASE:
// the HI / LO line (SMALL) right-aligned. The caps sit 8 rows apart: rule, clock, date, row, rule.
constexpr int DASH_SPLIT = 160;
constexpr int CLOCK_Y = 29, DATE_Y = 58, WORDS_Y = 58, HILO_BASE = 86, CLOCK_RULE_Y = 94;
// the trip: TRIP_LABEL (SMALL), the time (BIG) TRIP_TIME_GAP after it, the distance (BODY) right-
// aligned, all on TRIP_BASE, the BIG caps centred between the rules
constexpr const char* TRIP_LABEL = "TRIP";
constexpr int TRIP_BASE = 126, TRIP_TIME_GAP = 6, TRIP_RULE_Y = 135;
// the lights on one row (BODY, on LIGHT_ROW_BASE, the caps centred between the rules): INSIDE left of
// a vertical MID rule at LIGHTS_SPLIT, OUTSIDE right of it, each half from lightX: its label, a chip
// of its colour as tall as the caps (LIGHT_CHIP_W wide) LIGHT_GAP after it, and LIGHT_GAP after that
// what the light shows ("YELLOW 100%", "OFF", "-- NO LINK" with no chip)
constexpr const char* LIGHT_LABELS[] = {"IN", "OUT"};  // the controllers' order: inside, outside
constexpr int N_LIGHT_LABELS = sizeof(LIGHT_LABELS) / sizeof(LIGHT_LABELS[0]);
constexpr int LIGHT_ROW_BASE = 157, LIGHTS_RULE_Y = 166, LIGHTS_SPLIT = 159;
constexpr int LIGHT_CHIP_W = 14, LIGHT_CHIP_H = 11, LIGHT_GAP = 6;
// light blue's name on the row: "LIGHT BLUE 100%" doesn't fit OUT's half; CYAN (a word voice takes
// for it too) does
constexpr const char* LIGHT_BLUE_SHORT = "CYAN";
// the prompt line (SMALL, on PROMPT_BASE, the caps centred between the lights' rule and the items): a
// message while one shows, music mode's line, else PROMPT_IDLE; the block cursor (a SMALL cell)
// after it, by DASH_RIGHT
constexpr const char* PROMPT_IDLE = ">";
constexpr int PROMPT_BASE = 185;
// music mode's line: once the song's palette came, MUSIC_PROMPT and its swatch as a MUSIC_CHIP px
// square (SMALL's caps, on the baseline) MUSIC_CHIP_GAP after it and the swatch's name MUSIC_CHIP_GAP
// after that (">MUSIC [chip] PINK"); before, MUSIC_WAITING alone; with no palette for a while,
// MUSIC_NO_SIGNAL alone (not LYRICS's ">LYRICS: NO SIGNAL")
constexpr const char* MUSIC_PROMPT = ">MUSIC";
constexpr const char* MUSIC_WAITING = ">MUSIC: WAITING";
constexpr const char* MUSIC_NO_SIGNAL = ">MUSIC: NO SIGNAL";
constexpr int MUSIC_CHIP = 10, MUSIC_CHIP_GAP = 5;

// Whether the HI / LO line `s`, right-aligned at DASH_RIGHT, fits the row from DASH_X. When not,
// ledpanel shows it without its city.
inline bool hiLoFits(const char* s) { return DASH_X <= DASH_RIGHT - termfont::width(termfont::Face::SMALL, s); }

// Where the trip's time starts: after TRIP_LABEL.
inline int tripTimeX() { return DASH_X + termfont::advance(termfont::Face::SMALL, TRIP_LABEL) + TRIP_TIME_GAP; }

// Music mode's swatch chip and its name on the prompt line.
inline int songChipX() { return DASH_X + termfont::advance(termfont::Face::SMALL, MUSIC_PROMPT) + MUSIC_CHIP_GAP; }
inline int songNameX() { return songChipX() + MUSIC_CHIP + MUSIC_CHIP_GAP; }

// A light's half of the lights row (0 inside, 1 outside): where it starts, its chip, its text.
inline int lightX(int i) { return i == 0 ? DASH_X : LIGHTS_SPLIT + DASH_X; }
inline int lightChipX(int i) { return lightX(i) + termfont::advance(termfont::Face::BODY, LIGHT_LABELS[i]) + LIGHT_GAP; }
inline int lightTextX(int i) { return lightChipX(i) + LIGHT_CHIP_W + LIGHT_GAP; }

// The KARAOKE body: the line due now in BIG over up to 3 lines KARAOKE_BIG_DY apart (or, when that
// would cut it, TITLE over up to 4 lines KARAOKE_FIT_DY apart) in KARAOKE_BIG_H rows from KARAOKE_BIG_Y;
// the next line in BODY over up to 2 lines KARAOKE_NEXT_DY apart in KARAOKE_NEXT_H rows from
// KARAOKE_NEXT_Y; KARAOKE_W px wide. A BIG block holding Japanese (ui/lyricrun: 2x Unifont glyphs, 32
// rows centred on the caps) goes KARAOKE_WIDE_DY apart, 3 lines of it a little taller than the area;
// the big area's clear, rows KARAOKE_BIG_CLEAR_Y up to KARAOKE_BIG_CLEAR_END, covers every block with
// its accents.
constexpr int KARAOKE_W = 312;
constexpr int KARAOKE_BIG_Y = 56, KARAOKE_BIG_H = 90, KARAOKE_BIG_DY = 30, KARAOKE_FIT_DY = 22, KARAOKE_WIDE_DY = 32;
constexpr int KARAOKE_BIG_CLEAR_Y = 44, KARAOKE_BIG_CLEAR_END = 147;
constexpr int KARAOKE_NEXT_Y = 156, KARAOKE_NEXT_H = 40, KARAOKE_NEXT_DY = 20;
// The row of a big-area block's first line's tallest ASCII glyph: n lines dy apart, centred.
constexpr int karaokeBlockY(int n, int dy) { return KARAOKE_BIG_Y + (KARAOKE_BIG_H - n * dy) / 2; }

// The update page (an update over WiFi, util/otapage), the whole screen: the dashboard's header and
// its rule, the title (BIG, centred, caps top UPDATE_TITLE_Y), the 10-cell block bar (UPDATE_CELL px
// cells UPDATE_GAP apart, UPDATE_BAR_H tall, top UPDATE_BAR_Y, centred) and the % under it (TITLE,
// centred, caps top UPDATE_PCT_Y).
constexpr int UPDATE_TITLE_Y = 62;
constexpr int UPDATE_CELL = 22, UPDATE_GAP = 6, UPDATE_BAR_W = 10 * (UPDATE_CELL + UPDATE_GAP) - UPDATE_GAP;
constexpr int UPDATE_BAR_Y = 120, UPDATE_BAR_H = 28;
constexpr int UPDATE_PCT_Y = 166;

}  // namespace termlayout
