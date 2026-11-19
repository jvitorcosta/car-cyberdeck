#pragma once
#include <cstddef>
#include <cstdint>
#include "obdwire.h"
// The Suki drive screen (v19, full screen; from the research session's approved mock "Suki Dashboard R1
// Refined", then João's full-screen mock v19): the girl from the film monitor on the right, animated
// from real cels and scaled evenly to the deck's full height, and on the left a panel the deck draws
// itself in the theme's own colours: the SUKI letters, a speed card (three seven-segment cells and a
// short RPM trace), four equal tiles (km/l now, intake air, trip km, the estimated gear) and three
// identical bars (PEDAL, COOL, BATT) with 16-pill tracks.
// It follows the deck's theme (termpal's nine, day and night): the art is stored once as 8-bit indices
// into one palette, and the palette is recoloured per theme here (the mock's recolour(): hues move to
// the theme's, skin, black, white and grey stay).
// This is the file's reader, the recolour, the scene (what the values say) and the drawing of any
// rectangle of the screen into big-endian RGB565 pixels (what pushImage sends), so the view pushes
// only the regions whose key changed. PURE, host-tested.
// File "SKR2" (data/sukiride.bin, tools/gen_suki_ride.py, git-ignored: the art is the film's),
// little-endian: u16 art x, art w, art h (160, 160, 240: the girl's strip to the right edge), cel
// count (19), font count, logo w, h (108 x 26), 0; 256 x (r, g, b); per cel art w*h indices; the
// logo's logo w*h alpha bytes; per font u8 cell height, s8 tracking, u8 glyph count, u8 cap height and
// per glyph u8 char, u8 width, u16 0, u32 offset of its width*height alpha bytes in the file.
// Spec: docs/superpowers/specs/2026-11-19-suki-drive-screen-design.md
namespace sukiride {
constexpr int W = 320, H = 240;                        // the screen
constexpr int ART_X = 160, ART_W = 160, ART_H = 240;   // the girl's strip, at the right edge
constexpr int LOGO_W = 108, LOGO_H = 26;
constexpr int CELS = 19, COLOURS = 256;
constexpr int PANEL_W = 178, FADE_W = 30;              // the left panel; then it fades into the art
constexpr size_t HEAD = 20, FONT_HEAD = 4, GLYPH = 8;
enum FontId : uint8_t { F_SMALL, F_VALUE, F_GEAR, F_BIG, FONTS };

struct Font { uint8_t h = 0, n = 0, cap = 0; int8_t track = 0; const uint8_t* entries = nullptr; };
struct Art {
    const uint8_t* blob = nullptr;
    size_t len = 0;
    const uint8_t* pal = nullptr;    // COLOURS x (r, g, b)
    const uint8_t* cels = nullptr;   // CELS x ART_W * ART_H indices
    const uint8_t* logo = nullptr;   // LOGO_W * LOGO_H alpha
    Font font[FONTS];
};
// False when blob isn't a whole SKR2 file of this layout (sizes, CELS cels, FONTS fonts, glyphs inside).
bool parse(const uint8_t* blob, size_t len, Art* out);
int width(const Art& a, FontId f, const char* s);   // pixels, tracking included; unknown chars skipped

// ---- colours ----
// The mock's recolour of one art colour (0xRRGGBB): a colour with saturation >= 0.12 that isn't skin
// (hue 8..55, lightness > 0.72, saturation > 0.2) gets a new hue: the purple family (225..290) lands
// near hAcc, every other hue around h0, keeping half its spread; saturation x sat. Then x dim (night).
uint32_t recolour(uint32_t rgb, float h0, float hAcc, float sat, float dim);
float hueOf(uint32_t rgb);   // 0..360
// One theme's colours (big-endian RGB565, like the buffer): the art's palette, the interface's roles
// from termpal (TEXT, MID, DIM, ALERT), the tag's text (dark or white, the better contrast on ALERT)
// and the panel's background per row (the theme's DIM at 10 %, a touch lighter at the top and darker
// at the bottom). theme: termpal's swatch (0..8, -1 the default green); night: its night palette, the
// art at 78 %.
struct Look {
    uint16_t art[COLOURS];
    uint16_t tx, md, dm, al, onAlert, pipOff;
    uint16_t bg[H];
};
void makeLook(const Art& a, int theme, bool night, Look* out);

// ---- the values ----
enum class Zone : uint8_t { Cold, Norm, Warn, Alert };
Zone coolZone(float c);    // the theme's dim under 77 C, normal, warn from 95, alert from 105
Zone battZone(float v);    // alert under 12 V, normal, warn over 14.7
Zone pedalZone(float p);   // normal, warn from 85 %
constexpr int PILLS = 16, TRACE_N = 13;
// The bars' scales: the fraction of the track a value fills (clamped 0..1).
float coolFrac(float c);   // 60..130 C
float battFrac(float v);   // 11..15 V
float pedalFrac(float p);  // 0..100 %
int pillsLit(float frac);  // 0..PILLS (NAN: 0)

// The gear as an estimate: rpm / speed matched to seven bands (a CVT's seven stepped ratios in the
// City's S mode, from ~125 rpm per km/h in 1st to ~20 in 7th; to be calibrated from a drive log).
// 0 (shown as "-") under GEAR_MIN_KMH, when either is unknown or the ratio is out of every band.
constexpr float GEAR_MIN_KMH = 10.0f;
int gearEst(float rpm, float kmh);

// Which cel shows (the mock's celIdx): parked, frozen or no speed: the calm cel 7; under 80 km/h five
// calm cels ping-pong at 4 a second; 80..110 calm with the open mouth; over 110 the teeth cels shuffled
// 8 a second. shake: the 1 px sideways jitter over 110 (-1, 0 or 1).
int celIndex(float kmh, bool live, uint32_t nowMs);
int shake(float kmh, bool live, uint32_t nowMs);

// The last TRACE_N RPM samples, one every SAMPLE_MS (0 = none yet / unknown).
class Trace {
public:
    static constexpr uint32_t SAMPLE_MS = 250;
    void reset() { *this = Trace{}; }
    void update(float rpm, uint32_t nowMs);   // NAN keeps sampling, as 0
    uint16_t at(int i) const { return v_[i]; }   // 0 the oldest .. TRACE_N - 1 the newest
    uint32_t samples() const { return n_; }
private:
    uint16_t v_[TRACE_N] = {};
    uint32_t n_ = 0, at_ = 0;
    bool any_ = false;
};
constexpr float TRACE_RED = 6500.0f;

struct Ribbon {                  // one bar
    char val[8] = "--", unit[3] = "", label[6] = "";
    uint8_t lit = 0;             // pills on
    Zone zone = Zone::Norm;      // the lit pills' colour (the value's zone)
    Zone pill[PILLS] = {};       // each pill's own zone (the unlit ones' faint tint)
    bool bad = false;            // the alert (pulsing)
};
struct Chip { char label[5] = "", val[9] = "--", unit[3] = ""; };   // one tile; val: up to "99999.9"
// Everything the screen shows now. Built by scene(); compose() draws it.
struct Scene {
    bool live = false;           // the link is live: else every value "--", the screen dimmed, calm cel
    char speed[4] = "---";       // three cells, right-aligned (spaces), "---" without one
    char rpm[6] = "--";          // the RPM card's number
    uint16_t trace[TRACE_N] = {};
    uint8_t gear = 0;            // 0: "-"
    Chip chip[3];
    Ribbon rib[3];               // PEDAL, COOL, BATT top to bottom
    bool pulse = false;          // the alert's 3 Hz phase
    uint8_t cel = 7;
    int8_t shake = 0;
    bool demo = false;           // simulated values (GAUGES DEMO): the tag says DEMO, never mixed with real ones
};
// The red states come from the caller's drivedash::Alerts (coolant over 105 C, volts under 12 V for a
// minute with the engine running): coolBad / battBad.
void scene(const obdwire::Live& l, const Trace& t, bool coolBad, bool battBad, uint32_t nowMs, Scene* out);

// ---- drawing ----
struct Rect { int16_t x, y, w, h; };
// The live regions: what the view redraws when its key changes.
enum Region : uint8_t { R_CEL, R_SPEED, R_TRACE, R_GEAR, R_CHIP0, R_CHIP1, R_CHIP2, R_RIB0, R_RIB1, R_RIB2, REGIONS };
Rect region(Region r);
uint32_t key(const Scene& s, Region r);   // changes whenever what r shows changes
constexpr int CEL_REGION_X = PANEL_W - 2;               // the art's region: the fade and the girl
constexpr int MAX_PIXELS = (W - CEL_REGION_X) * H;      // the biggest region: the view's buffer
constexpr int STRIP_H = MAX_PIXELS / W;                 // a whole-screen redraw goes in strips this tall
// Rectangle r of the screen (inside 0..W x 0..H) into buf (r.w * r.h big-endian RGB565): the panel and
// the art, then every element over it, dimmed to 65 % when the link isn't live.
void compose(const Art& a, const Look& k, const Scene& s, Rect r, uint16_t* buf);
}  // namespace sukiride
