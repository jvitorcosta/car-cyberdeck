#pragma once
// Where the yellow deck's car-LED panel (ledpanel.cpp) puts its text, as far as fitting goes: the
// x of each fixed label in its rect, the boot's typed lines and the dashboard's bands. Shared with test_termfont, which checks every fixed label fits at its termfont Face with
// room to spare. PURE, host-tested.
#include "termfont.h"

namespace termlayout {

// The Menu's three items (100 px wide), in deckmenu::Item order: the label (BODY) top left, its caps'
// top ROW_TEXT_Y and its x ROW_TEXT_X from the rect's; under it, right-aligned, the item's tag while
// it has one (SMALL, an inverted mini-label: TAG_PAD px of fill around the caps, TAG_RIGHT in from
// the item's right edge, TAG_BOTTOM up from its bottom): KARAOKE's and MUSIC MODE's ON or OFF (always
// one of them). "[KARAOKE]" and a tag side by side don't fit 100 px, so the tag goes under the label, and the label always sits high (it doesn't jump when a tag comes).
constexpr const char* MENU_ITEMS[] = {"[LIGHTS]", "[KARAOKE]", "MUSIC MODE"};
constexpr int N_MENU_ITEMS = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
constexpr int ROW_TEXT_X = 8, ROW_TEXT_Y = 7;
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

// The boot: three SMALL lines typed out on a blank screen, caps tops BOOT_Y0 + i * BOOT_DY, then the
// build's FW_LABEL (util/version) under them as line N_BOOT. A line's pitch is its caps and its
// descent ('Q', ','), so no descender lands on the next line's caps.
constexpr const char* BOOT_LINES[] = {"WELCOME TO CYBERDECK", ">SET TERMINAL/INQUIRE",
                                      "HONDA CITY // CARDECK OS"};
constexpr int N_BOOT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
constexpr int BOOT_X = 8, BOOT_Y0 = 2, BOOT_DY = 13;

// ---- The Menu: a dashboard of stacked bands (deckmenu's HEADER, CLOCK_BAND, LIGHTS_BAND, PROMPT_BAND,
// then the items at the bottom), each band but the prompt line under a MID rule from x 4
// to 315 (from the clock band down, the next band's first row). Text starts at DASH_X; right-aligned
// text ends at DASH_RIGHT. A _Y is the caps' top row, a _BASE the baseline (the row under the caps),
// as ledpanel's text() places them.
constexpr int DASH_X = 8, DASH_RIGHT = 312;
// the header: one line (SMALL) and its rule
constexpr const char* DASH_TITLE = "HONDA CITY TOURING";
constexpr int DASH_TITLE_Y = 5, HEADER_RULE_Y = 20;
// right-aligned on the title's row while a PS3 controller is connected: PAD_LABEL (SMALL, MID), then
// PAD_GAP after it a battery icon as tall as the caps (PAD_ICON_W wide, a PAD_NUB_W x PAD_NUB_H nub on
// its right), PAD_CELL_W px cells PAD_CELL_GAP apart inside it (util/yellow/padbattery)
constexpr const char* PAD_LABEL = "PAD";
constexpr int PAD_GAP = 4, PAD_ICON_W = 19, PAD_NUB_W = 2, PAD_NUB_H = 4, PAD_CELL_W = 3, PAD_CELL_GAP = 1;
// The header's PAD part, label to nub, from its left edge to DASH_RIGHT.
inline int padWidth() { return termfont::width(termfont::Face::SMALL, PAD_LABEL) + PAD_GAP + PAD_ICON_W + PAD_NUB_W; }
// the clock (BIG) with the date (SMALL) under it left of DASH_SPLIT; the trip's time (BIG) with
// TRIP_LABEL (SMALL) under it right of it, right-aligned; the band ends at CLOCK_RULE_Y.
constexpr int DASH_SPLIT = 160;
constexpr int CLOCK_Y = 58, DATE_Y = 87, WORDS_Y = 87, CLOCK_RULE_Y = 135;
constexpr const char* TRIP_LABEL = "TRIP";
// the lights on one row (BODY, on LIGHT_ROW_BASE, the caps centred between the rules): INSIDE left of
// a vertical MID rule at LIGHTS_SPLIT, OUTSIDE right of it, each half from lightX: its label, a chip
// of its colour as tall as the caps (LIGHT_CHIP_W wide) LIGHT_GAP after it, and LIGHT_GAP after that
// what the light shows ("YELLOW 100%", "OFF", "-- NO LINK" with no chip)
constexpr const char* LIGHT_LABELS[] = {"IN", "OUT"};  // the controllers' order: inside, outside
constexpr int N_LIGHT_LABELS = sizeof(LIGHT_LABELS) / sizeof(LIGHT_LABELS[0]);
constexpr int LIGHT_ROW_BASE = 157, LIGHTS_RULE_Y = 166, LIGHTS_SPLIT = 159;
constexpr int LIGHT_CHIP_W = 14, LIGHT_CHIP_H = 11, LIGHT_GAP = 6;
// the prompt line (SMALL, on PROMPT_BASE, the caps centred between the lights' rule and the items): a
// message while one shows, music mode's line, else PROMPT_IDLE; the block cursor (a SMALL cell)
// after it, by DASH_RIGHT
constexpr const char* PROMPT_IDLE = ">";
constexpr int PROMPT_BASE = 185;
// music mode's line: once the song's palette came, MUSIC_PROMPT and its swatch as a MUSIC_CHIP px
// square (SMALL's caps, on the baseline) MUSIC_CHIP_GAP after it and the swatch's name MUSIC_CHIP_GAP
// after that (">MUSIC [chip] PINK"); before, MUSIC_WAITING alone; with no palette for a while,
// MUSIC_NO_SIGNAL alone
constexpr const char* MUSIC_PROMPT = ">MUSIC";
constexpr const char* MUSIC_WAITING = ">MUSIC: WAITING";
constexpr const char* MUSIC_NO_SIGNAL = ">MUSIC: NO SIGNAL";
constexpr int MUSIC_CHIP = 10, MUSIC_CHIP_GAP = 5;

// Music mode's swatch chip and its name on the prompt line.
inline int songChipX() { return DASH_X + termfont::advance(termfont::Face::SMALL, MUSIC_PROMPT) + MUSIC_CHIP_GAP; }
inline int songNameX() { return songChipX() + MUSIC_CHIP + MUSIC_CHIP_GAP; }

// A light's half of the lights row (0 inside, 1 outside): where it starts, its chip, its text.
inline int lightX(int i) { return i == 0 ? DASH_X : LIGHTS_SPLIT + DASH_X; }
inline int lightChipX(int i) { return lightX(i) + termfont::advance(termfont::Face::BODY, LIGHT_LABELS[i]) + LIGHT_GAP; }
inline int lightTextX(int i) { return lightChipX(i) + LIGHT_CHIP_W + LIGHT_GAP; }

}  // namespace termlayout
