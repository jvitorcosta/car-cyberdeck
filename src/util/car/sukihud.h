#pragma once
#include <cstddef>
#include <cstdint>
// The Suki look of the gauge screen (black/ui/sukiview): the film monitor's art from a file on the
// deck (data/suki.bin, tools/gen_suki_hud.py, git-ignored: the art is the film's), live values drawn
// over it. This is the file's reader, the screen's layout and the drawing into a rectangle's pixels
// (big-endian RGB565, as pushImage sends them), so the view only pushes what changed.
// File "SUK1" (little-endian): u16 w, h (320 x 240), u16 font count, u16 0; then w*h big-endian
// RGB565 (the background, every label already on it); then per font a u8 cell height, s8 tracking,
// u8 glyph count, u8 0 and per glyph u8 char, u8 width, u16 0, u32 offset of its pixels in the file:
// width*height triples (RGB565 high byte, low byte, alpha 0..255). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Suki look")
namespace sukihud {
constexpr int W = 320, H = 240;
constexpr size_t HEAD = 12, FONT_HEAD = 4, GLYPH = 8;
enum FontId : uint8_t { F_SMALL, F_BAR, F_CLOCK, F_RPM, F_LOAD, F_SPEED, FONTS };

struct Font { uint8_t h = 0, n = 0; int8_t track = 0; const uint8_t* entries = nullptr; };
struct Art { const uint8_t* blob = nullptr; size_t len = 0; const uint8_t* bg = nullptr; Font font[FONTS]; };
// False when blob isn't a whole SUK1 file of W x H with all FONTS fonts (glyphs inside the file).
bool parse(const uint8_t* blob, size_t len, Art* out);
int width(const Art& a, FontId f, const char* s);   // pixels, tracking included; unknown chars skipped

struct Rect { int16_t x, y, w, h; };
// The screen's live regions: what the view clears (to the background) and redraws when it changes.
constexpr Rect WAVE{9, 53, 173, 46};       // inside the waveform window: the last 12 s of RPM
constexpr Rect BAT{37, 107, 27, 10}, THR{89, 107, 26, 10};    // the FD / HD lines' values
constexpr Rect H2O{37, 116, 27, 10}, IAT{89, 116, 26, 10};
constexpr Rect CLOCK{148, 107, 56, 18};    // right-aligned at x 202
constexpr Rect STAGE3{14, 126, 86, 14}, STAGE2{27, 145, 86, 14}, STAGE1{40, 165, 86, 14};   // 0-100, BEST, KM/L
constexpr Rect SPEED{6, 190, 104, 36}, TACH{110, 192, 156, 33}, RPM{108, 226, 64, 12};
constexpr Rect LOAD{284, 189, 32, 48};
constexpr int MAX_PIXELS = 173 * 48;   // at least the biggest region (WAVE): the view's one buffer

// r's background into buf (r.w * r.h pixels).
void background(const Art& a, Rect r, uint16_t* buf);
enum class Align : uint8_t { Left, Right, Center };
// s in font f, blended over buf (region r, screen coordinates), x the left / right edge / centre,
// yMid the text's middle; clipped to r.
void text(const Art& a, FontId f, Rect r, uint16_t* buf, int x, int yMid, const char* s, Align al);
// One of the film's capsules (a vertical pill) over buf: rim colour, and a core line when core != 0
// (big-endian colours, like the buffer's). Clipped to r.
void capsule(Rect r, uint16_t* buf, int cx, int y0, int y1, int w, uint16_t rim, uint16_t core);

// The tach: TACH_N capsules growing to the right, red from SHIFT_RPM.
constexpr int TACH_N = 20;
constexpr float TACH_MAX = 7000.0f, SHIFT_RPM = 6000.0f;
int tachLit(float rpm);                    // 0..TACH_N (NAN: 0)
bool tachHot(int i);                       // capsule i is in the shift zone
int tachX(int i);                          // its centre
int tachTop(int i);                        // its top (the bottom is TACH_BOTTOM)
constexpr int TACH_BOTTOM = 223;
// The load capsule (LOAD_W x LOAD_H, its bottom at LOAD_BOTTOM): the fill's height, 0..LOAD_H, for
// 0..100 % (NAN: 0). A fill shorter than LOAD_W is drawn as narrow as it is tall (a pill stays a pill).
constexpr int LOAD_W = 20, LOAD_H = 30, LOAD_CX = 300, LOAD_BOTTOM = 222;
int loadFill(float pct);
constexpr float LOAD_HOT = 85.0f;
}  // namespace sukihud
