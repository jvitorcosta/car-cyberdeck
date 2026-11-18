#pragma once
#include <cstddef>
#include <cstdint>
#include "obdplan.h"
// CITY DATALINK, the black deck's live gauge screen (black/ui/gaugeview): the maths behind what it
// draws. A ramp of SEGS slanted blocks for the RPM, growing taller to the right, green / amber / red
// by the rpm at each block's lower edge, with a peak marker that holds, then falls; a SHIFT tag that
// blinks from 85 % of the redline; numbers as text ("--" when unknown); 10-cell bars for throttle and
// load; the tiles' warnings. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md
namespace gaugehud {
constexpr int SEGS = 24;
constexpr float RPM_MAX = 7000.0f;    // the ramp's full scale
constexpr float REDLINE = 6500.0f;    // the City's 1.5 i-VTEC; check against its own tach
constexpr float AMBER_FROM = 5000.0f;

enum class Zone : uint8_t { Green, Amber, Red };

int lit(float rpm);                    // blocks lit, 0..SEGS (NAN or under one block: 0)
Zone zone(int block);                  // 0..SEGS-1
int height(int block, int hMin, int hMax);   // the ramp: hMin for block 0 up to hMax for the last

// The peak marker: on the highest block reached, held HOLD_MS, then down one block per FALL_MS,
// never under the blocks lit now.
class Peak {
public:
    static constexpr uint32_t HOLD_MS = 1500, FALL_MS = 60;
    int update(int litNow, uint32_t nowMs);
private:
    int peak_ = 0;
    uint32_t atMs_ = 0;
};

bool shiftOn(float rpm, uint32_t nowMs);   // from obdplan::rpmWarn, blinking at 4 Hz
// v rounded to `decimals` into out; "--" for NAN or when it doesn't fit in n. Never "-0".
void text(float v, int decimals, char* out, size_t n);
int cells(float pct, int n);               // a 0..100 % bar's lit cells, rounded, clamped
bool coolWarn(float c);                    // obdplan::coolantWarn; NAN: no
bool battWarn(float volts, float rpm);     // obdplan::batteryWarn, engine running when rpm > 0; NAN volts: no
const char* linkTag(obdplan::Link l);      // OFF / SEARCH / KEY ON / LIVE

// The speed's own 7-segment digits (drawn, not a font: the unlit segments show faintly).
// Segment bits: a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle.
enum : uint8_t { SEG_A = 1, SEG_B = 2, SEG_C = 4, SEG_D = 8, SEG_E = 16, SEG_F = 32, SEG_G = 64 };
uint8_t segments(char c);                  // '0'..'9', '-'; anything else (a space) is blank
// The speed as 3 right-aligned characters: " 62", "118"; " --" when unknown, "---" past 999.
void speedCells(float kmh, char out[4]);
}  // namespace gaugehud
