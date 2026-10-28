#pragma once
#include <cstdint>
#include <cmath>
// The car health check's instant side: what the Watch can say about the battery and the engine from the
// live readings alone (no history beyond a hold timer and a trip mean). One table of defaults; the
// screen turns a Verdict into a colour and a line. Unknown readings (NAN / -1) never warn. PURE, host-tested.
namespace carhealth {
enum class Level : uint8_t { Ok, Note, Problem };
enum class Item : uint8_t { Battery, Engine, COUNT };

struct Reading {  // NAN / -1 = unknown
    uint32_t nowMs = 0;
    bool engineRunning = false;
    float volts = NAN;  // ATRV
    float coolantC = NAN, ltftPct = NAN;
    int storedCodes = -1, pendingCodes = -1;
    bool mil = false;
};

struct Verdict {
    Level level = Level::Ok;
    char why[48] = "";  // "" when Ok; plain words, caps, <= 47 chars
};

constexpr float BATT_LOW_V = 12.0f;
constexpr uint32_t BATT_HOLD_MS = 60000;
constexpr float COOLANT_HOT_C = 105.0f;
constexpr float LTFT_NOTE = 10.0f, LTFT_PROBLEM = 20.0f;

class Watch {
public:
    void feed(const Reading& r);  // ~1 Hz
    Verdict item(Item i) const;   // the worst live condition for the item
    bool critical() const;        // coolant hot or MIL: may draw while moving
    void endTrip();               // LTFT-for-a-whole-trip is judged here
private:
    Reading last_;
    bool haveLast_ = false;
    bool lowActive_ = false;  // volts under BATT_LOW_V while running, since lowSinceMs_
    uint32_t lowSinceMs_ = 0;
    float ltftSum_ = 0.0f;  // sum and count of the running trip so far
    uint32_t ltftN_ = 0;
    Verdict ltftVerdict_;  // the last finished trip's, kept until the next one ends
};
}  // namespace carhealth
