#pragma once
#include <cstddef>
#include <cstdint>
// The gauge screens' choice and the Drive dash's maths (screen 2 of the CarDeck mocks): one dominant
// number, the speed in seven-segment digits over ghost 888s; a 20-segment RPM arc above it; coolant,
// load and volts ladders under it; a warm-up bar. The view (black/ui/driveview) draws what this says.
// PURE, host-tested. Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Drive dash")
namespace drivedash {

// The GAUGES screens. Suki: the film monitor (ui/sukiview, the street look without its art);
// Drive: this dash; Trip: the Trip ticker (util/car/tripticker, ui/tripview); Ride: the Suki drive
// screen R1.1 (util/car/sukiride, ui/sukirideview: the film art recoloured, the girl animated). Kept in
// NVS ("hud" / "screen") as the number.
enum class Screen : uint8_t { Suki = 0, Drive = 1, Trip = 2, Ride = 3, Cockpit = 4, COUNT };
const char* name(Screen s);                     // "suki", "drive", "trip", "ride", "cockpit"
bool parse(const char* text, Screen* out);      // a name or its number ("drive", "2"); false otherwise
// Switching screens is for a parked car (the mocks' screen rules): refused over MOVING_KMH. An unknown
// speed (no link) counts as parked.
constexpr float MOVING_KMH = 3.0f;
bool canSwitch(float kmh);

// Seven segments, bit 0 = a (top) .. bit 6 = g (middle): what a character lights. Unknown: none.
uint8_t segments(char c);
struct Box { int16_t x, y, w, h; };
// Segment k (0..6, a..g) of a digit cell at (x, y), w x h, strokes t thick.
Box segment(int k, int x, int y, int w, int h, int t);

// The RPM arc: N segments over the top of a circle, FULL rpm lights them all; from HOT_FROM on they are
// red (the last 20 %: over 5600 rpm, past i-VTEC's crossover).
constexpr int ARC_N = 20, ARC_CX = 160, ARC_CY = 118, ARC_R = 88, ARC_W = 10, HOT_FROM = 16;
constexpr float ARC_FULL = 7000.0f;
int arcLit(float rpm);               // 0..ARC_N; 0 for NAN
bool arcHot(int i);
// Segment i's quad: outer and inner corners at its two ends (x0..x3, y0..y3: outer start, outer end,
// inner end, inner start), pixels.
struct Quad { int16_t x[4], y[4]; };
Quad arcQuad(int i);

// A ladder of `cells`: how many v of max lights (0 for NAN or v <= 0).
int ladderLit(float v, float max, int cells);
// Warm-up: coolant 40 C empty .. 85 C full, 0..1; 0 for NAN.
float warmup(float coolC);

// The red states, with hysteresis so the colours don't flip: coolant from over 105 C until under
// 100 C; volts after 60 s under 12.0 V with the engine running, until 12.2 V or more. NAN changes
// nothing.
class Alerts {
public:
    void update(float coolC, float volts, float rpm, uint32_t nowMs);
    bool coolant() const { return cool_; }
    bool volts() const { return volt_; }
    static constexpr float COOL_ON = 105.0f, COOL_OFF = 100.0f, VOLT_ON = 12.0f, VOLT_OFF = 12.2f;
    static constexpr uint32_t VOLT_MS = 60000;
private:
    bool cool_ = false, volt_ = false, low_ = false;
    uint32_t lowSince_ = 0;
};

// The RPM in thousands with one decimal ("2.4"), "--" for NAN. out: 5 bytes or more.
void rpmText(float rpm, char* out, size_t n);
}  // namespace drivedash
