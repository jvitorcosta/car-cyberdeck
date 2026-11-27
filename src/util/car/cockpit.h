#pragma once
#include <cstddef>
#include <cstdint>
#include "obdwire.h"
// The Cockpit gauge screen (the "Split Cockpit, full height" of the 2026-11 mock rounds): no header row,
// an 8 px margin, VT323 terminal type. Left: the speed in three big seven-segment cells (112 px tall),
// KM/H and the RPM number, a 16-capsule tach with a 0-7 scale, trip and range. Right, past a hairline
// divider: five rows, COOL, BATT, KM/L (the trip's average), PEDAL, LOAD, each a label, a value and a thin
// 14-tick bar whose danger part turns red; on an alert the row's label and value sit black on a block of the
// alert colour (the tick bar stays on black under it). A CHK (check-engine lamp) or NO LINK / KEY ON block of
// that colour sits over the speed's first cells (CHK is small enough to stay clear of a lit digit). Otherwise no
// filled boxes, like the yellow deck: pure black, lines and type in the LED colour; the only fills are alerts and that pill. No
// clock, no LIVE tag, no wordmark. Its colours and its black are the yellow deck's (the INSIDE light swatch's
// termpal night palette).
// This is the screen's layout, its text, the palette and the regions the view repaints. PURE,
// host-tested; black/ui/cockpitview draws it.
// Spec: docs/superpowers/specs/2026-11-20-cockpit-screen-design.md
namespace cockpit {
constexpr int W = 320, H = 240, MARGIN = 8;

// ---- geometry (screen pixels) ----
constexpr int SPD_X = 8, SPD_Y = 8, SPD_CW = 54, SPD_CH = 112, SPD_T = 13, SPD_GAP = 5, SPD_CELLS = 3;
constexpr int DIVIDER_X = 184;                                              // the hairline between the two sides
constexpr int PILL_X = 12, PILL_Y = 12, PILL_H = 22;                        // the status pill over the first digit
constexpr int LINE_BASE = 148;                                              // KM/H, RPM and the number's baseline
constexpr int KMH_X = 8, RPM_LABEL_X = 86, RPM_RIGHT = 182, RPM_BASE = 150;
constexpr int TACH_X = 8, TACH_Y = 156, TACH_W = 174, TACH_H = 20, TACH_N = 16, TICK_BASE = 189;
constexpr int TRIP_X0 = 8, TRIP_X1 = 100, TRIP_LABEL_BASE = 209, TRIP_VALUE_BASE = 232;
constexpr int ROWS = 5, ROW_X = 190, ROW_W = 122, ROW_H = 42, PILLS = 14;
constexpr int TICK_DY = ROW_H - 7, ALERT_FILL_H = TICK_DY - 2;    // a row's tick bar; the alert block above it
inline int rowY(int i) { return 8 + (i * 91 + 1) / 2; }                     // 8, 54, 99, 145, 190 (pitch 45.5)

// The five cards, top to bottom.
enum Row : uint8_t { COOL, BATT, KMPL, PEDAL, LOAD };

struct Rect { int16_t x, y, w, h; };
// What the view repaints on its own: a region whose key() did not change is left alone.
enum Region : uint8_t { R_SPEED, R_RPM, R_TACH, R_TRIP, R_ROW0, R_ROW1, R_ROW2, R_ROW3, R_ROW4, REGIONS };
Rect region(Region r);

// ---- the values ----
enum class Zone : uint8_t { Norm, Warn, Alert };
enum class Status : uint8_t { None, Chk, NoLink, KeyOn };   // the pill over the speed's first digit

struct Card {
    char label[6] = "";
    char val[8] = "--";     // the value with its unit ("88\xB0", "13.9V", "41%", "11.2"); "--" when unknown
    uint8_t lit = 0;        // pills on, 0..PILLS
    uint8_t hot = PILLS;    // the first pill of the danger part (PILLS: none)
    bool bad = false;       // the alert: label and value black on the alert colour
};
struct Scene {
    bool live = false;
    char speed[4] = "---";  // three cells, right-aligned (spaces); "---" without a value
    char rpm[6] = "--";
    uint8_t tachLit = 0;    // capsules on, 0..TACH_N
    Zone rpmZone = Zone::Norm;
    Card card[ROWS];
    char trip[8] = "--", range[8] = "--";
    Status status = Status::None;
    uint8_t codes = 0;      // the stored-code count (0..99): kept for a later screen; the pill says CHK only
};
// The red states come from the caller's drivedash::Alerts. storedCodes: obdlink's count (-1 unknown),
// mil: the check-engine lamp; both count only while the link is live.
void scene(const obdwire::Live& l, bool coolBad, bool battBad, int storedCodes, bool mil, Scene* out);

// The tach: capsule i of TACH_N (0 the left one), lit when i < tachLit; its zone is the rpm at its
// upper edge: Alert over 6500, Warn over 5000.
Zone tachZone(int i);
int tachLit(float rpm);               // 0..TACH_N; 0 for NAN or rpm <= 0
// The pills a bar's fraction lights (rounded, clamped; 0 for NAN).
int pillsLit(float frac);
// A hash of what region r draws; equal keys mean the same pixels.
uint32_t key(const Scene& s, Region r);

// ---- colours ----
// The yellow deck's colours, so the two decks match: its `pal()` is termpal::rgb(role, theme, true), the theme's night
// palette at any hour, on termpal's BG (pure black). theme: the yellow deck's INSIDE light swatch (termpal's 0..8,
// -1 the default green), as karaoke::themeLight() reports it. RGB565 in the panel's native order (a sprite's draw calls).
struct Palette {
    uint16_t bg;               // termpal's BG: black
    uint16_t tx, md, dm;       // termpal's TEXT, MID, DIM
    uint16_t al;               // the alert (fills, bar)
    uint16_t ghost;            // unlit segments, capsules and pills (DIM at 25 %)
    uint16_t warn, core;       // the tach's bright band, and a capsule's core line (TEXT lifted toward white)
    uint16_t border;           // the divider line (DIM at 65 %)
};
void makePalette(int theme, Palette* out);
}  // namespace cockpit
