#pragma once
// Where the yellow deck's car-LED panel (ledpanel.cpp) puts its text, as far as fitting goes: the
// x of each fixed label in its rect, the boot's typed lines and the dashboard's bands. Shared with test_termfont, which checks every fixed label fits at its termfont Face with
// room to spare. PURE, host-tested.
#include "termfont.h"

namespace termlayout {

// The Menu's two switches (KARAOKE, MUSIC: 154x64, layout B), in deckmenu::Item order: the label
// (BODY) at the left, its caps' top ROW_TEXT_Y (centred top to bottom) and its x ROW_TEXT_X from the
// rect's; bottom right the switch's tag (SMALL, an inverted mini-label: TAG_PAD px of fill around the
// caps, TAG_RIGHT in from the rect's right edge, TAG_BOTTOM up from its bottom): ON or OFF, always one.
// LIGHTS is the lights panel (its hint names it), so MENU_ITEMS[LIGHTS] only names it in the log.
constexpr const char* MENU_ITEMS[] = {"LIGHTS", "KARAOKE", "MUSIC MODE"};
constexpr int N_MENU_ITEMS = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
constexpr int ROW_TEXT_X = 8, ROW_TEXT_Y = 26;
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

// A preset on the PRESETS tab (100x72): the name (TITLE, caps top PRESET_NAME_Y) centred over two
// PRESET_CHIP px square chips (inside, outside) PRESET_CHIP_GAP apart, their top PRESET_CHIP_Y; all
// from the rect's top / centre.
constexpr int PRESET_NAME_Y = 15, PRESET_CHIP = 18, PRESET_CHIP_GAP = 10, PRESET_CHIP_Y = 39;

// The prompt line, every screen's bottom row (deckmenu::PROMPT_BAND): a MID rule across at
// PROMPT_RULE_Y, then the line (SMALL, on PROMPT_BASE: as low as its descenders allow, so accented
// capitals clear the rule) and the block cursor after it. Text from DASH_X.
constexpr int PROMPT_RULE_Y = 219, PROMPT_BASE = 237;

// The boot: three SMALL lines typed out on a blank screen, caps tops BOOT_Y0 + i * BOOT_DY, then the
// build's FW_LABEL (util/version) under them as line N_BOOT. A line's pitch is its caps and its
// descent ('Q', ','), so no descender lands on the next line's caps.
constexpr const char* BOOT_LINES[] = {"WELCOME TO CYBERDECK", ">SET TERMINAL/INQUIRE",
                                      "HONDA CITY // CARDECK OS"};
constexpr int N_BOOT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
constexpr int BOOT_X = 8, BOOT_Y0 = 2, BOOT_DY = 13;

// ---- The Menu: a dashboard of stacked bands (deckmenu's HEADER, CLOCK_BAND), each under a MID
// rule from x 4 to 315, then the lights panel and the two switches, then the prompt line. Text starts at DASH_X; right-aligned
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
// The link tags on the title's row (SMALL): TAG_GPS, LINK_TAG_GAP, TAG_DECK, right-aligned at
// DASH_RIGHT, or two SMALL cells left of the PAD part while a controller is connected. MID when
// fine, ALERT with a 1 px strike through the caps' middle when not, DIM when not known (ledpanel).
constexpr const char* TAG_GPS = "GPS";
constexpr const char* TAG_DECK = "DECK";
constexpr int LINK_TAG_GAP = 6;
inline int linkTagsW() {
    return termfont::width(termfont::Face::SMALL, TAG_GPS) + LINK_TAG_GAP + termfont::width(termfont::Face::SMALL, TAG_DECK);
}
inline int linkTagsX(bool pad) {
    return DASH_RIGHT - linkTagsW() - (pad ? padWidth() + 2 * termfont::cell(termfont::Face::SMALL) : 0);
}
// One row: the clock (TITLE) and CLOCK_GAP after it the date (SMALL) left of DASH_SPLIT; right of
// it, right-aligned, the trip's time (TITLE) and CLOCK_GAP before it TRIP_LABEL (SMALL). The caps
// are centred between the header's rule and CLOCK_RULE_Y, the SMALL words on the TITLE caps' bottom.
constexpr int DASH_SPLIT = 160;
constexpr int CLOCK_Y = 29, DATE_Y = 33, WORDS_Y = 33, CLOCK_RULE_Y = 52, CLOCK_GAP = 8;
constexpr const char* TRIP_LABEL = "TRIP";
inline int dateX() { return DASH_X + termfont::width(termfont::Face::TITLE, "88:88") + CLOCK_GAP; }
// The lights panel (the Menu's LIGHTS button, deckmenu::rect(Menu, LIGHTS), outlined): INSIDE left
// of a vertical MID rule at PANEL_SPLIT, OUTSIDE right of it, each half from lightX: a block of its
// colour LIGHT_BLOCK_W x LIGHT_BLOCK_H from LIGHT_BLOCK_Y, and LIGHT_GAP after it its label (SMALL,
// caps top LIGHT_LABEL_Y) over what it shows (LIGHT_VALUE_FACE on LIGHT_VALUE_BASE: "YELLOW 100%",
// "OFF", "-- NO LINK" with no block). TITLE fits "PURPLE 100%" only with a block 28 px wide
// (test_termfont), so the block grew taller, not wider. Bottom right PANEL_HINT (SMALL, DIM), its
// right end at PANEL_HINT_RIGHT, on PANEL_HINT_BASE: the panel opens the Lights screen.
constexpr const char* LIGHT_LABELS[] = {"INSIDE", "OUTSIDE"};  // the controllers' order
constexpr int N_LIGHT_LABELS = sizeof(LIGHT_LABELS) / sizeof(LIGHT_LABELS[0]);
constexpr int PANEL_SPLIT = 160;
constexpr int LIGHT_BLOCK_W = 28, LIGHT_BLOCK_H = 52, LIGHT_BLOCK_Y = 70, LIGHT_GAP = 6;
constexpr int LIGHT_LABEL_Y = 70, LIGHT_VALUE_BASE = 122;
constexpr termfont::Face LIGHT_VALUE_FACE = termfont::Face::TITLE;
constexpr const char* PANEL_HINT = "LIGHTS >";
constexpr int PANEL_HINT_RIGHT = DASH_RIGHT - 4, PANEL_HINT_BASE = 140;
// the prompt line's idle text on the Menu (with no message and music mode off)
constexpr const char* PROMPT_IDLE = ">";
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

// A light's half of the lights panel (0 inside, 1 outside): where it starts (its block), its text.
inline int lightX(int i) { return i == 0 ? 12 : PANEL_SPLIT + 8; }
inline int lightTextX(int i) { return lightX(i) + LIGHT_BLOCK_W + LIGHT_GAP; }

}  // namespace termlayout
